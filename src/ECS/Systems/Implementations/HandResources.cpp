/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"

#include "Audio/Services/Guidance.h"
#include "ECS/TakeResource.h"

#include "HandSystemDetail.h"

#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <tuple>

#include <fmt/format.h>
#include <glm/gtc/type_ptr.hpp>

#include <spdlog/spdlog.h>

#include <L3DFile.h>
#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Archetypes/HandArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "Camera/Camera.h"
#include "Windowing/WindowingInterface.h"
#include "Camera/CameraModel.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Alpha.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/MorphWithTerrain.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/MapCells.h"
#include "ECS/PotResource.h"
#include "ECS/Components/Sprite.h"
#include "Graphics/Texture2D.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/ObjectResources.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "LandBalance.h"
#include "ECS/Trees.h"
#include "GameClock.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

float HandSystem::HeldFill() const noexcept
{
	if (!_held)
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* pot = registry.TryGet<const Pot>(*_held); pot != nullptr && pot->maxAmount > 0)
	{
		return std::clamp(static_cast<float>(pot->amount) / static_cast<float>(pot->maxAmount), 0.0f, 1.0f);
	}
	return 0.0f;
}

PotInfo HandSystem::PotInfoOf(entt::entity entity) noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity))
	{
		return PotInfo::_COUNT;
	}
	const auto* pot = registry.TryGet<const Pot>(entity);
	if (pot == nullptr)
	{
		return PotInfo::_COUNT;
	}
	if (pot->type != PotInfo::_COUNT)
	{
		return pot->type;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	const auto meshId = registry.Get<const Mesh>(entity).id;
	for (size_t i = 0; i < pots.size(); ++i)
	{
		if (resources::HashIdentifier(pots[i].meshId) == meshId)
		{
			return static_cast<PotInfo>(i);
		}
	}
	return PotInfo::_COUNT;
}

bool HandSystem::ProcessInInteractPile() noexcept
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pile = registry.TryGet<const Pot>(*_held);
	if (!registry.AllOf<Pot>(*_pickSource) || pile == nullptr)
	{
		return false;
	}
	// PileResource::ProcessInInteract (0x66E520), once per game turn while the locked select lasts (no distance
	// check). The values come from the hand pot's info:
	//   ticks = (1000 / msPerTurn) * multiPickUpRampTime, t = clamp(n / ticks, 0, 1)
	//   amount = (int)(perTurn + (perTurnEnd - perTurn) * t^2), limited by the source and maxAmountCanBePickedUp.
	const auto handType = PotInfoOf(*_held);
	const bool wood = handType == PotInfo::HandWood;
	const auto& info = Locator::infoConstants::value().pot[static_cast<size_t>(wood ? PotInfo::HandWood : PotInfo::HandFood)];
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	// fn_0066CD30: t = n / GPotInfo::GetTicksToChangeOver 0x66CD00 (ftol(1000 / [0xD01A38] * ramp time), the
	// NumGameTicksPerSecond conversion), <= 0 (or NaN) gives 0 and >= 1 gives 1
	const auto ticks = static_cast<float>(game_clock::TicksForSeconds(info.multiPickUpRampTime));
	const float ratio = static_cast<float>(_pickTurns) / ticks;
	const float t = ratio > 0.0f ? std::min(ratio, 1.0f) : 0.0f;
	auto take = static_cast<uint32_t>(static_cast<float>(info.amountPickedUpPerTurn) +
	                                  static_cast<float>(info.amountPickedUpPerTurnEnd - info.amountPickedUpPerTurn) * t * t);
	const uint32_t room = info.maxAmountCanBePickedUp > pile->amount ? info.maxAmountCanBePickedUp - pile->amount : 0u;
	// GetResource / RemoveResource (vt +0x98 / +0xA0) of the source with the hand's interface: a storage pit's pile
	// takes from the pit, a loose pile from itself (JustRemoveFromPot: emptied, it goes through ToBeDeleted), and the
	// hand's branch (desire, alignment, belief) runs
	const uint32_t available = object_resources::GetResource(*_pickSource, resource);
	take = std::min({take, available, room, 65535u - pile->amount});
	if (take == 0)
	{
		return false;
	}
	object_resources::RemoveResource(*_pickSource, resource, take,
	                                  pot_resource::Dropper {true, PlayerNames::PLAYER_ONE, true});
	auto& handPot = registry.Get<Pot>(*_held); // the source's deletion may have moved the pool
	handPot.amount = static_cast<uint16_t>(handPot.amount + take);
	// UpdateMultiPickup(type, t^2): the looping pick-up sound's pitch, 60 + 180 t^2 percent (UpdatePickupSound)
	_pickupSoundFraction = t * t;
	registry.SetDirty();
	const bool sourceLeft = ecs::IsAvailable(*_pickSource);
	if (std::getenv("OPENBLACK_HAND_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Pick trace: turn {} hand pile {}, source left {}", _pickTurns, handPot.amount,
		                   sourceLeft ? static_cast<int>(object_resources::GetResource(*_pickSource, resource)) : -1);
	}
	// an emptied loose pile is gone: the next turn's IsAvailable would end the select
	return sourceLeft;
}

void HandSystem::SinkPile(entt::entity pile) noexcept
{
	// Every Just{Add,Remove}Resource calls SetSize: piles ease to their new sink offset, plain pots rescale.
	archetypes::PotArchetype::SetSize(pile, true);
}

void HandSystem::PutDownHandPot(entt::entity pot) noexcept
{
	// Pot::ApplyThisToObject 0x66DDD0 on the land: Pot::AddResourceToPos 0x66F270 (ECS/PotResource) at the pot's position
	// with its amount, IsPoisoned and no speed-up, from the local player's interface. It merges into the stores and
	// same-resource pots of the 3x3 cells around (each within 2 x its 2D radius, 1.2 for a store), else it makes a
	// MagicWood / MagicFood pile.
	auto& registry = Locator::entitiesRegistry::value();
	const auto type = PotInfoOf(pot);
	const bool wood = type == PotInfo::HandWood;
	const auto& held = registry.Get<Pot>(pot);
	const auto amount = held.amount;
	const bool poisoned = held.poisoned;
	auto position = registry.Get<Transform>(pot).position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	registry.Destroy(pot);
	registry.SetDirty();
	// 0x66F27A: off the map (MapCoords::InBounds) nothing is added anywhere: the resource is lost (pot_resource's
	// AddResourceToPos checks it, and drops what is left over on water, 0x66F42D)
	if (amount == 0)
	{
		return;
	}
	const auto resource = wood ? ResourceType::Wood : ResourceType::Food;
	const pot_resource::Dropper dropper {true, PlayerNames::PLAYER_ONE, true};
	entt::entity pile = entt::null;
	const auto put = pot_resource::AddResourceToPos(position, dropper, resource, amount, poisoned, false, &pile);
	if (pile != entt::null)
	{
		// Pot::ApplyThisToMapCoord (0x66DED8): the food reaction, once (the hungry grazers come to eat)
		ecs::animal_ai::SetupPotReaction(pile);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Hand: put down {} {} ({} into stores or pots there)", amount, wood ? "wood" : "food", put);
}

std::optional<entt::entity> HandSystem::FindWoodStore(glm::vec3 point) const noexcept
{
	const auto& registry = Locator::entitiesRegistry::value();
	std::optional<entt::entity> store;
	float best = std::numeric_limits<float>::max();
	registry.Each<const StoragePit, const Transform>([&](entt::entity entity, const StoragePit&, const Transform& transform) {
		if (!ecs::IsAvailable(entity))
		{
			return;
		}
		float radius = 8.0f;
		if (const auto* fixed = registry.TryGet<const Fixed>(entity); fixed != nullptr)
		{
			radius = std::max(radius, fixed->boundingRadius);
		}
		const float distance = glm::distance(glm::vec2(point.x, point.z), glm::vec2(transform.position.x, transform.position.z));
		if (distance <= radius && distance < best)
		{
			best = distance;
			store = entity;
		}
	});
	return store;
}

void HandSystem::DepositInStore(entt::entity object, entt::entity store, const pot_resource::Dropper& is) noexcept
{
	// openblack's roots of an uprooted tree go first (they are not part of the original's tree)
	DropRoots(object, false);
	// StoragePit::DeleteObjectAndTakeResource 0x733750 (ecs::take_resource): the Supply help trigger (0x7337A6),
	// then Object::DoDeleteObjectAndTakeResource 0x63A940 (ecs::object_delivery: the wood taken, the sounds,
	// ToBeDeleted), then the reaction 0x16 (0x7337BA)
	ecs::take_resource::StoragePit(store, object, is);
	Locator::entitiesRegistry::value().SetDirty();
}
