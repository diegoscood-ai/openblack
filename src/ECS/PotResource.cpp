/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "PotResource.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <chrono>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <LNDFile.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Audio/Audio.h"
#include "Audio/Guidance.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Map.h"
#include "ECS/MapCoords.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"
#include "PSys/PSysManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
bool Trace()
{
	static const bool trace = std::getenv("OPENBLACK_SPELL_TRACE") != nullptr || std::getenv("OPENBLACK_HAND_TRACE") != nullptr;
	return trace;
}

/// The MapCoords of a point (ftol(x * 6553.6)); its high words are the 10 m cells
glm::ivec2 MapCoordsOf(const glm::vec3& position)
{
	return {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z)};
}

/// MapCoords::ToMap 0x603430: the cell, or none out of the 512 x 512 map
std::optional<glm::ivec2> CellOf(glm::ivec2 coords)
{
	const map_coords::MapCoords at {coords.x, coords.y, 0.0f};
	const uint16_t side = Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide() : 512;
	if (!map_coords::InBounds(at, side))
	{
		return std::nullopt;
	}
	return map_coords::Cell(at);
}

const lnd::LNDCell* LandCellOf(const glm::vec3& position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return nullptr;
	}
	const auto cell = CellOf(MapCoordsOf(position));
	if (!cell || position.x < 0.0f || position.z < 0.0f)
	{
		return nullptr;
	}
	return &Locator::terrainSystem::value().GetCell(glm::u16vec2(*cell));
}

bool InHandOrFlying(entt::entity entity)
{
	// An object in the hand or in physics is out of the map lists
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	const auto& hand = Locator::handSystem::value();
	if (const auto held = hand.GetHeldObject(); held && *held == entity)
	{
		return true;
	}
	const auto thrown = hand.GetThrownObjects();
	return std::find(thrown.begin(), thrown.end(), entity) != thrown.end();
}

ResourceType ResourceOf(const Pot& pot)
{
	if (pot.type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return ResourceType::None;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
}

/// What one object in the cell lists does with the resource: IsResourceStore(type) (vt 0x680) or IsPot && GetResourceType
/// == type, within Get2DRadius x the multiplier of pos (IsCloseToEqual 0x6053C0: GetDistanceInMetres <= r), then
/// AddResource (vt 0x9C). Returns what it took.
uint32_t OfferTo(entt::entity object, const glm::vec3& position, ResourceType type, uint32_t left, bool poisoned)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return 0;
	}
	const bool store = registry.AllOf<StoragePit>(object);
	auto* pot = registry.TryGet<Pot>(object);
	const auto owner = pot != nullptr ? StoragePitStore::OwnerOf(object) : entt::null;
	if (!store)
	{
		// StoragePit::IsResourceStore 0x55CD20 answers 1 for any type; PotStructure::IsResourceStore 0x66DA30 asks the
		// structure it is part of (and the pile's own type); Pot::IsResourceStore 0x66F560 is 0, then IsPot and the type
		if (pot == nullptr || ResourceOf(*pot) != type)
		{
			return 0;
		}
	}
	// GetDefaultFireCentrePos: the object's position (Object 0x639AA0)
	const float radius = pot_resource::Get2DRadius(object) * pot_resource::RadiusMultiplierForApplyingPotToPos(object);
	const float distance = glm::distance(glm::vec2(position.x, position.z), glm::vec2(transform->position.x, transform->position.z));
	if (!(distance <= radius))
	{
		return 0;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: {} of {} into {} {} (at {:.2f} m, radius {:.2f})", left,
		                   type == ResourceType::Wood ? "wood" : "food", store || owner != entt::null ? "the store of" : "pile",
		                   static_cast<uint32_t>(store ? object : (owner != entt::null ? owner : object)), distance, radius);
	}
	if (store || owner != entt::null)
	{
		// StoragePit::AddResource 0x732F60 / PotStructure::AddResource 0x66ED70 -> the store.
		// 0x732F67..0x732F99: before anything else, if the pit's +0x74 is not null and the type is WOOD (1) or ANY (-2),
		// the whole call is forwarded to that object's AddResource (vt 0x9C) and this function returns its answer.
		// +0x74 is MultiMapFixed::building_site, a BuildingSite* (bw1-decomp src/Black/MultiMapFixed.h; StoragePit
		// derives from Abode, whose own fields only start at 0x7C): a pit that is being built sends its wood to its
		// building site. (pendiente) openblack has no building sites and nothing builds abodes (ECS/Components/Town.h),
		// so no pit can ever have one and the redirect is unreachable; port it with the building sites.
		return StoragePitStore::AddResource(store ? object : owner, type, left);
	}
	// PotStructure::AddResource -> JustAddResource (vt 0x8C). PileResource::JustAddResource 0x66D330 plays the pile sound
	// with the amount added (not for the hand's pots, infos 11 and 12); Pot::JustAddResource 0x66D2B0 clips at maxInPot
	// only when nextPotForResource < 19 (no magic or loose pile), SetPoisoned(poisoned || IsPoisoned), SetSize (vt 0x85C)
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	if (info.potType != PotType::Pot && pot->type != PotInfo::HandWood && pot->type != PotInfo::HandFood)
	{
		pot_resource::PlayPileSound(object, transform->position, type, left);
	}
	uint32_t add = left;
	if (static_cast<int32_t>(info.nextPotForResource) < 19 && pot->amount + add > info.maxAmountInPot)
	{
		add = info.maxAmountInPot > pot->amount ? info.maxAmountInPot - pot->amount : 0u;
	}
	add = std::min<uint32_t>(add, 65535u - pot->amount); // openblack's guard: Pot::amount is a uint16 here
	pot->amount = static_cast<uint16_t>(pot->amount + add);
	pot->poisoned = poisoned || pot->poisoned;
	archetypes::PotArchetype::SetSize(object, true);
	return add;
}

/// A cell's lists in the order AddResourceToPos walks them: the fixed list (+4), then the mobile one (+0). The original
/// lists' order is not known: here each list is in entity order (inf).
std::vector<entt::entity> CellObjects(glm::ivec2 cell)
{
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> fixed;
	if (Locator::entitiesMap::has_value())
	{
		const MapInterface::CellId id(static_cast<uint16_t>(cell.x), static_cast<uint16_t>(cell.y));
		for (const auto entity : Locator::entitiesMap::value().GetFixedInGridCell(id))
		{
			if (registry.Valid(entity) && registry.AllOf<StoragePit>(entity))
			{
				fixed.push_back(entity);
			}
		}
	}
	std::sort(fixed.begin(), fixed.end());
	// the mobile list: the pots (MobileObjects), found by their position so that a pile made this turn counts at once
	std::vector<entt::entity> mobile;
	registry.Each<const Pot, const Transform>([&](entt::entity entity, const Pot&, const Transform& transform) {
		if (transform.position.x < 0.0f || transform.position.z < 0.0f)
		{
			return;
		}
		const auto at = CellOf(MapCoordsOf(transform.position));
		if (at && *at == cell && !InHandOrFlying(entity))
		{
			mobile.push_back(entity);
		}
	});
	std::sort(mobile.begin(), mobile.end());
	fixed.insert(fixed.end(), mobile.begin(), mobile.end());
	return fixed;
}
} // namespace

float pot_resource::PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	float p = static_cast<float>(amount) / static_cast<float>(std::max(1u, maxInPot));
	p = p < 0.0f ? 0.0f : 0.05f + (1.0f - 0.05f) * std::min(p, 1.0f);
	return std::clamp(1.0f - (1.0f - p) * (1.0f - p), 0.0f, 1.0f);
}

float pot_resource::Get2DRadius(entt::entity object)
{
	const float radius = effects::Object2DRadius(object);
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pot = registry.TryGet<const Pot>(object);
	if (pot == nullptr || pot->type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return radius;
	}
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	// PileFood::Get2DRadius 0x66F180 (the potType 1 piles; the hand's food keeps its own GetHoldRadius)
	return info.potType == PotType::PileFood ? radius * PileFoodProportionRaised(pot->amount, info.maxAmountInPot) : radius;
}

float pot_resource::RadiusMultiplierForApplyingPotToPos(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Pot>(object) ? 2.0f : 1.2f;
}

bool pot_resource::IsWater(const glm::vec3& position)
{
	// MapCoords::IsWater 0x6035B0 (0x603617: out of the 512 x 512 cells or without a land block the answer is 1, the
	// open sea). The single source is ecs::sea_cells (no cell or no block = water, else the cell's water bit).
	return sea_cells::IsWater(position);
}

bool pot_resource::IsDryLand(const glm::vec3& position)
{
	const auto* cell = LandCellOf(position);
	return cell != nullptr && cell->altitude >= 4;
}

int pot_resource::PileSoundSample(ResourceType type, uint32_t amount, uint32_t t)
{
	if (amount < 200)
	{
		return type == ResourceType::Food ? 77 + static_cast<int>(t % 6) : 92 + static_cast<int>(t % 6);
	}
	return type == ResourceType::Food ? 75 + static_cast<int>(t & 1) : 86 + static_cast<int>(t % 6);
}

void pot_resource::PlayPileSound(entt::entity pile, const glm::vec3& position, ResourceType type, uint32_t amount)
{
	// fn_0066D1A0: GetTickCount picks the sample (0x66D1D7 / 0x66D1E7: one call for the food sample and one for the wood
	// one; a single reading here), then GAudio::PlaySoundEffect 0x429E30 (0x66D26A) with bank InGame (Global +0x3AC),
	// owner the pile (this, +0x20), is3D 1, track 0, at the MapCoords' point
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), PileSoundSample(type, amount, audio::TickCount())};
	options.owner = audio::Owner::Thing(pile);
	options.is3D = true;
	options.track = false;
	options.position = position;
	audio::PlaySoundEffect(options);
}

void pot_resource::SetSpeedUp(entt::entity pile, bool on)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(pile);
	if (pot == nullptr)
	{
		return;
	}
	const bool was = pot->speedUp;
	pot->speedUp = on;
	const bool food = pot->type != PotInfo::_COUNT && Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).potType ==
	                                                        PotType::PileFood;
	// PotStructure::SetSpeedUp 0x55D530 only keeps the flag; PileFood's also shows it
	if (!food || on == was)
	{
		return;
	}
	if (pot->speedUpVisual != entt::null && registry.Valid(pot->speedUpVisual))
	{
		registry.Destroy(pot->speedUpVisual); // GParticleContainer::CloseDown
	}
	pot->speedUpVisual = entt::null;
	if (on)
	{
		// CreateSpotVisualWithSpecifiedDuration(pos, PILEFOOD_SPEEDUP (46), 1.0, -1, pile): for ever, on the pile (the
		// 1.0 is taken as the scale, the -1 as the duration: inf)
		const auto position = registry.Get<const Transform>(pile).position;
		pot->speedUpVisual = psys::manager::CreateSpotVisual(46, position, -1.0f, pile);
	}
}

uint32_t pot_resource::AddResourceToPos(const glm::vec3& position, const Dropper& dropper, ResourceType type, uint32_t amount,
                                        bool poisoned, bool speedUp, entt::entity* newPile)
{
	if (newPile != nullptr)
	{
		*newPile = entt::null;
	}
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value() || position.x < 0.0f || position.z < 0.0f)
	{
		return 0;
	}
	auto coords = MapCoordsOf(position);
	if (!CellOf(coords))
	{
		return 0; // MapCoords::InBounds 0x6042C0
	}
	uint32_t left = amount;
	map_coords::Spiral spiral; // GUtils::Spiral 0x74D7E0
	for (int i = 0; i < 9; ++i)
	{
		if (const auto cell = CellOf(coords); cell)
		{
			for (const auto object : CellObjects(*cell))
			{
				if (left == 0)
				{
					break;
				}
				const uint32_t taken = OfferTo(object, position, type, left, poisoned);
				left -= std::min(taken, left);
				// TODO(M8): DoCreatureMimicAfterAddingResource (vt 0x68C) when there is an interface
			}
		}
		// MapCoords += JustMapXZ 0x605470: one cell
		const auto& step = spiral.Next();
		coords += glm::ivec2(step.x, step.z) * map_coords::k_FixedPerCell;
	}
	if (left == 0 || IsWater(position))
	{
		return amount - left;
	}
	const auto player = dropper.hasInterface ? std::optional(dropper.player) : std::nullopt;
	const auto pile = magic::objects::CreateMagicResourcePile(position, player, type, left);
	if (pile == entt::null)
	{
		return amount - left;
	}
	if (newPile != nullptr)
	{
		*newPile = pile;
	}
	auto& registry = Locator::entitiesRegistry::value();
	PlayPileSound(pile, registry.Get<const Transform>(pile).position, type, left);
	auto& pot = registry.Get<Pot>(pile);
	pot.poisoned = poisoned || pot.poisoned; // SetPoisoned (vt 0x69C)
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: new {} pile {} of {} at ({:.1f}, {:.1f}), 2D radius {:.2f}",
		                   type == ResourceType::Wood ? "MagicWood" : "MagicFood", static_cast<uint32_t>(pile), left, position.x,
		                   position.z, Get2DRadius(pile));
	}
	SetSpeedUp(pile, speedUp || pot.speedUp); // vt 0x864
	// Pot::AddResourceToPos 0x66F4D8..0x66F509 calls GGuidance::ResourceDropSFX 0x71B570 only with a status that is
	// GGame::MyInterfaceStatus 0x555880 (this is `dropper.isMyInterface`), passing the drop point and a RESOURCE_RAIN_TYPE:
	// 1 for food (type 0) and 2 for wood (type 1), 0 for anything else (0x66F4EB..0x66F502). 0x71B570 then: GGuidance::
	// PlayNow(1); the nearest town within 100 m ([0x98013C], MapCoords::GetNearestTown 0x6020E0); GetResourceDropSample
	// 0x71B5F0 (the town's three need floats; above 0.5 [0x980140] 0x1352..0x1354 food / 0x1355..0x1357 wood, above 0.25
	// [0x980144] 0x135B..0x135D food, wood keeps the same three); GGuidance::PlaySample 0x71C6F0 (Audio/Guidance.*)
	if (dropper.isMyInterface)
	{
		const auto rain = type == ResourceType::Wood   ? audio::guidance::RainType::Wood
		                  : type == ResourceType::Food ? audio::guidance::RainType::Food
		                                               : audio::guidance::RainType::None;
		audio::guidance::ResourceDropSFX(position, rain);
	}
	return amount - left; // 0x66F511: eax = amount - left on every path, so the new pile's part is not counted
}
