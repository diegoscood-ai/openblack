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

#include <LNDFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "Common/GUtilsDistance.h"
#include "Creature/LocalPlayer.h"
#include "Debug/DebugEnv.h"
#include "Debug/TraceLog.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/SeaCells.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicPiles.h"
#include "Particles/ParticleManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
bool Trace()
{
	static const bool trace = debug_env::SpellTrace() || debug_env::HandTrace();
	return debug_env::TraceLogs(trace);
}

/// The MapCoords of a point (x * 6553.6, truncated); its high words are the 10 m cells
glm::ivec2 MapCoordsOf(const glm::vec3& position)
{
	return {map_coords::ToFixed(position.x), map_coords::ToFixed(position.z)};
}

/// The cell, or none out of the 512 x 512 map
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

ResourceType ResourceOf(const Pot& pot)
{
	if (pot.type == PotInfo::_COUNT || !Locator::infoConstants::has_value())
	{
		return ResourceType::None;
	}
	return Locator::infoConstants::value().pot.at(static_cast<size_t>(pot.type)).resourceType;
}

/// How one object in the cell lists answers a put-down of `type`: none when it neither stores the type nor is a pot or
/// a pile of it. The centre is the object's position, a worship site's own centre; the reach is its 2D radius times its
/// multiplier
std::optional<pot_resource::PutDownCandidate> CandidateOf(entt::entity object, ResourceType type)
{
	// what the original has out of the map (in the hand, flying, gone) is not in its lists
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr || !map_cells::IsReadable(object))
	{
		return std::nullopt;
	}
	const auto* pot = registry.TryGet<const Pot>(object);
	pot_resource::PutDownCandidate candidate {
	    .storesType = resource_stores::IsResourceStore(object, type),
	    .isPot = pot != nullptr,
	    .potType = pot != nullptr ? ResourceOf(*pot) : ResourceType::None,
	};
	if (!candidate.storesType && !(candidate.isPot && candidate.potType == type))
	{
		return std::nullopt;
	}
	candidate.centre = registry.AllOf<WorshipSite>(object) ? object::WorshipSiteCentre(object) : transform->position;
	candidate.radius2D = pot_resource::Get2DRadius(object);
	candidate.reachMultiplier = pot_resource::RadiusMultiplierForApplyingPotToPos(object);
	return candidate;
}

/// An object that takes the put-down is given what is left through its own AddResource (a storage pit, a workshop, a
/// worship site, a building and its site, a scaffold, a pot or a pile, each as it takes a resource), with the interface
/// that put it down and the poison. Then, with an interface, the object tells the creature what was done, whatever it
/// took (a storage pit, a workshop, a worship site or a building going up; a pot, a pile or a scaffold tells nothing).
/// Returns what it took
uint32_t GiveTo(entt::entity object, const glm::vec3& position, ResourceType type, uint32_t left, bool poisoned,
                const pot_resource::Dropper& dropper)
{
	const uint32_t taken = object_resources::AddResource(object, type, left, dropper, poisoned, &position);
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: {} of {} into {} at ({:.1f}, {:.1f}), took {}", left,
		                   type == ResourceType::Wood ? "wood" : "food", static_cast<uint32_t>(object), position.x, position.z,
		                   taken);
	}
	if (dropper.hasInterface)
	{
		object_resources::ReportResourceDeed(object, type, dropper);
	}
	return taken;
}

} // namespace

float pot_resource::PileFoodProportionRaised(uint32_t amount, uint32_t maxInPot)
{
	return object::PileFoodProportionRaised(amount, maxInPot);
}

float pot_resource::Get2DRadius(entt::entity object)
{
	return object::Get2DRadius(object);
}

float pot_resource::RadiusMultiplierForApplyingPotToPos(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Pot>(object) ? k_PotReachMultiplier : k_ObjectReachMultiplier;
}

bool pot_resource::TakesPutDown(const PutDownCandidate& candidate, const glm::vec3& position, ResourceType type)
{
	// a store of the type, or else a pot or a pile of that very type
	if (!candidate.storesType && !(candidate.isPot && candidate.potType == type))
	{
		return false;
	}
	// the reach is the multiplier times the 2D radius, rounded to a float, and the distance within it counts
	const float reach = candidate.reachMultiplier * candidate.radius2D;
	return gutils::GetDistanceInMetres(position, candidate.centre) <= reach;
}

bool pot_resource::IsWater(const glm::vec3& position)
{
	// out of the 512 x 512 cells or without a land block the answer is true, the open sea. The single source is
	// ecs::sea_cells (no cell or no block = water, else the cell's water bit).
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

int pot_resource::PileSoundSample(ResourceType type, uint32_t amount, uint32_t foodTicks, uint32_t woodTicks)
{
	const int food = PileSoundSample(ResourceType::Food, amount, foodTicks);
	const int wood = PileSoundSample(ResourceType::Wood, amount, woodTicks);
	return type == ResourceType::Food ? food : wood;
}

void pot_resource::PlayPileSound(entt::entity pile, const glm::vec3& position, ResourceType type, uint32_t amount)
{
	// the clock picks the sample: it is read twice, first for the food sample and then for the wood one, and the type
	// takes its own; then the sound with bank InGame, owner the pile, 3D, not tracked, at the MapCoords' point
	const uint32_t foodTicks = audio::TickCount();
	const uint32_t woodTicks = audio::TickCount();
	audio::PlayOptions options;
	options.sample = {audio::Bank(audio::SfxBank::InGame), PileSoundSample(type, amount, foodTicks, woodTicks)};
	options.owner = audio::Owner::Thing(pile);
	options.is3D = true;
	options.track = false;
	options.position = position;
	audio::PlaySoundEffect(options);
}

pot_resource::Dropper pot_resource::DropperOf(std::optional<PlayerNames> giver)
{
	if (!giver.has_value())
	{
		return {};
	}
	// my interface is the local player's; only the local hand keeps a record of who owned what it last picked up
	const bool mine = *giver == openblack::creature::LocalPlayer();
	return {.hasInterface = true,
	        .player = *giver,
	        .isMyInterface = mine,
	        .sourceOwner =
	            mine && Locator::handSystem::has_value() ? Locator::handSystem::value().GetSourceOwner() : std::nullopt};
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
	const bool food = pot->type != PotInfo::_COUNT &&
	                  Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).potType == PotType::PileFood;
	// other pots only keep the flag; a food pile also shows it
	if (!food || on == was)
	{
		return;
	}
	if (pot->speedUpVisual != entt::null && registry.Valid(pot->speedUpVisual))
	{
		registry.Destroy(pot->speedUpVisual); // the particle container closes down
	}
	pot->speedUpVisual = entt::null;
	if (on)
	{
		// the PILEFOOD_SPEEDUP spot visual (46) for ever, on the pile (the 1.0 is taken as the scale, the -1 as the
		// duration: infinite)
		const auto position = registry.Get<const Transform>(pile).position;
		pot->speedUpVisual =
		    Locator::particleSystem::value().StartSpotVisual(SpotVisualType::PilefoodSpeedup, position, -1, pile);
	}
}

uint32_t pot_resource::PotStructureAddResource(entt::entity object, ResourceType type, uint32_t amount, bool poisoned,
                                               const Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (pot == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto owner = StoragePitStore::OwnerOf(object);
	if (owner != entt::null)
	{
		// the pot of a pit -> the store, which first sends WOOD (1) or ANY (-2) to its building site while the pit is
		// being built, and answers with the site's answer (StoragePitStore::AddResource). The interface that put it
		// down and the poison go on to the pit
		return StoragePitStore::AddResource(owner, type, amount, dropper, poisoned);
	}
	// any other pot or pile takes it itself
	return pot_resource::AddToPotDirect(object, type, amount, poisoned);
}

uint32_t pot_resource::StoragePitTakesPutDownResource(entt::entity store, ResourceType type, uint32_t amount,
                                                      const Dropper& dropper, bool poisoned)
{
	// the pit's AddResource with the interface that put it down and the poison: with a town, its desire, alignment,
	// belief and deed
	const uint32_t taken = StoragePitStore::AddResource(store, type, amount, dropper, poisoned);
	// then the put-down's own deed, whatever was taken
	if (dropper.hasInterface)
	{
		object_resources::ReportResourceDeed(store, type, dropper);
	}
	return taken;
}

uint32_t pot_resource::AddToPotDirect(entt::entity object, ResourceType type, uint32_t amount, bool poisoned)
{
	// a pile plays the pile sound with the amount asked (not for the hand's pots, infos 11 and 12); the amount is
	// clipped at maxInPot only when nextPotForResource < 19 (no magic or loose pile), then poisoned if either was, and
	// the size
	auto& registry = Locator::entitiesRegistry::value();
	auto* pot = registry.TryGet<Pot>(object);
	const auto* transform = registry.TryGet<const Transform>(object);
	if (pot == nullptr || transform == nullptr)
	{
		return 0;
	}
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	if (info.potType != PotType::Pot && pot->type != PotInfo::HandWood && pot->type != PotInfo::HandFood)
	{
		pot_resource::PlayPileSound(object, transform->position, type, amount);
	}
	// the room left is unsigned and unchecked: a pot already over its maximum is brought back to it, and the call
	// answers the wrapped room
	uint32_t add = amount;
	if (static_cast<int32_t>(info.nextPotForResource) < 19 && pot->amount + add > info.maxAmountInPot)
	{
		add = info.maxAmountInPot - pot->amount;
	}
	pot->amount += add;
	pot->poisoned = poisoned || pot->poisoned;
	archetypes::PotArchetype::SetSize(object, true);
	return add;
}

uint32_t pot_resource::AddResourceToPos(const glm::vec3& position, const Dropper& dropper, ResourceType type, uint32_t amount,
                                        bool poisoned, bool speedUp, entt::entity* newPile)
{
	if (newPile != nullptr)
	{
		*newPile = entt::null;
	}
	if (!Locator::entitiesRegistry::has_value() || !Locator::infoConstants::has_value() || position.x < 0.0f ||
	    position.z < 0.0f)
	{
		return 0;
	}
	auto coords = MapCoordsOf(position);
	if (!CellOf(coords))
	{
		return 0; // out of bounds
	}
	uint32_t left = amount;
	map_coords::Spiral spiral;
	for (int i = 0; i < 9; ++i)
	{
		if (const auto cell = CellOf(coords); cell && left != 0)
		{
			// the cell's fixed list, then its mobile one, each from its head; in the original every new pile is in its cell's
			// fixed list at once, as it is here for the piles made with the cells' insertion (PotArchetype::Create, the magic
			// piles)
			const glm::ivec2 at = *cell;
			left = OfferAlongCell(
			    entt::entity {entt::null}, [at] { return map_cells::FirstFixed(at); },
			    [at] { return map_cells::FirstMobile(at); },
			    [at](entt::entity object) { return map_cells::GetMapChild(object, at); }, position, type, left,
			    [type](entt::entity object) { return CandidateOf(object, type); },
			    [&](entt::entity object, uint32_t rest) { return GiveTo(object, position, type, rest, poisoned, dropper); });
		}
		// one cell along the spiral
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
	pot.poisoned = poisoned || pot.poisoned;
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pot trace: new {} pile {} of {} at ({:.1f}, {:.1f}), 2D radius {:.2f}",
		                   type == ResourceType::Wood ? "MagicWood" : "MagicFood", static_cast<uint32_t>(pile), left,
		                   position.x, position.z, Get2DRadius(pile));
	}
	SetSpeedUp(pile, speedUp || pot.speedUp);
	// the guidance resource drop sound only for the local player's interface (`dropper.isMyInterface`), with the drop
	// point and a rain type: 1 for food (type 0) and 2 for wood (type 1), 0 for anything else. The guidance then picks
	// a sample by the needs of the nearest town within 100 m (Audio/Guidance.*)
	if (dropper.isMyInterface)
	{
		const auto rain = type == ResourceType::Wood   ? audio::guidance::RainType::Wood
		                  : type == ResourceType::Food ? audio::guidance::RainType::Food
		                                               : audio::guidance::RainType::None;
		audio::guidance::PlayResourceDropRemark(position, rain);
	}
	return amount - left; // amount - left on every path, so the new pile's part is not counted
}
