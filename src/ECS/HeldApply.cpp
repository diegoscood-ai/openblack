/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HeldApply.h"

#include <vector>

#include "3D/MapCoords.h"
#include "ECS/Abodes.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Systems/AnimatedStaticSystemInterface.h"
#include "ECS/Systems/MapCellsSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Scenery/AnimatedStaticRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

held_apply::ApplyAction held_apply::HeldClassApply(HeldKind held, const TargetFacts& target)
{
	switch (held)
	{
	case HeldKind::Tree:
	case HeldKind::DeadTree:
		// over a worship totem it is a sacrifice, which the hand gives before this (ecs::altar_offering)
		return target.storesHeldType ? ApplyAction::GiveToStore : ApplyAction::None;
	case HeldKind::Pot:
		if (target.storesHeldType)
		{
			return ApplyAction::GiveToStore;
		}
		return target.sameTypePot ? ApplyAction::MergeIntoPot : ApplyAction::None;
	case HeldKind::Animal:
		// over a worship totem it is a sacrifice, which the hand gives before this (ecs::altar_offering)
		return HeldClassValid(held, target) && !target.heldIndestructible && target.storesHeldType ? ApplyAction::GiveToStore
		                                                                                           : ApplyAction::None;
	case HeldKind::Fence:
		return target.storesHeldType && !target.heldIndestructible ? ApplyAction::GiveToStore : ApplyAction::None;
	case HeldKind::GateStone:
		return target.gateStonePlinth ? ApplyAction::LayInPlinth : ApplyAction::None;
	case HeldKind::Other:
		return ApplyAction::None;
	}
	return ApplyAction::None;
}

bool held_apply::HeldClassValid(HeldKind held, const TargetFacts& target)
{
	if (held == HeldKind::Animal)
	{
		// only a storage pit, whatever it holds
		return target.storagePit;
	}
	if (held == HeldKind::Fence)
	{
		return target.storesHeldType;
	}
	return HeldClassApply(held, target) != ApplyAction::None;
}

bool held_apply::AnimalImpactTakes(bool hitAvailable, bool selfAvailable, bool hitStoresFood)
{
	return hitAvailable && selfAvailable && hitStoresFood;
}

bool held_apply::FenceImpactTakes(bool isFence, bool indestructible, bool hitStoresWood)
{
	return isFence && !indestructible && hitStoresWood;
}

pot_resource::Dropper held_apply::ThrowerInterface(std::optional<PlayerNames> throwerPlayer)
{
	return pot_resource::DropperOf(throwerPlayer);
}

bool held_apply::PotImpactTakes(bool hitAvailable, bool potAvailable, const TargetFacts& hit)
{
	return hitAvailable && potAvailable && (hit.storesHeldType || hit.sameTypePot);
}

bool held_apply::TreeImpactTakes(bool hitValid, bool hitStoresWood)
{
	return hitValid && hitStoresWood;
}

int held_apply::ApplyResult(ApplyAction action, bool done)
{
	return action != ApplyAction::None && done ? k_ApplyConsumed : 0;
}

help_profile::Event held_apply::ApplyHelpEvent(bool sacrificeAltar)
{
	return sacrificeAltar ? help_profile::Event::Sacrifice : help_profile::Event::Supply;
}

std::optional<size_t> held_apply::BuildingSiteAmong(std::span<const SiteCandidate> fixed)
{
	// every object of the list is looked at, and a later one replaces an earlier one
	std::optional<size_t> found;
	for (size_t i = 0; i < fixed.size(); ++i)
	{
		const auto& candidate = fixed[i];
		if (!candidate.multiCell || (candidate.built && candidate.repaired))
		{
			continue;
		}
		if (!candidate.hasShape || candidate.shapeTouches)
		{
			found = i;
		}
	}
	return found;
}

bool held_apply::ReleaseLooksForBuildingSite(HeldKind held)
{
	switch (held)
	{
	case HeldKind::Tree:
	case HeldKind::DeadTree:
	case HeldKind::Pot:
	case HeldKind::Fence:
		return true;
	case HeldKind::Animal:
	case HeldKind::GateStone:
	case HeldKind::Other:
		return false;
	}
	return false;
}

bool held_apply::ReleasedGoesToSite(bool siteFound, bool siteStoresHeldType)
{
	return siteFound && siteStoresHeldType;
}

held_apply::HeldKind held_apply::KindOfHeld(entt::entity held)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (held == entt::null || !registry.Valid(held))
	{
		return HeldKind::Other;
	}
	if (registry.AllOf<Tree>(held))
	{
		return HeldKind::Tree;
	}
	if (registry.AllOf<DeadTree>(held))
	{
		return HeldKind::DeadTree;
	}
	if (registry.AllOf<Pot>(held))
	{
		return HeldKind::Pot;
	}
	if (registry.AllOf<Animal>(held))
	{
		return HeldKind::Animal;
	}
	if (registry.AllOf<MobileStatic>(held) && ecs::physics::from_hand::IsFence(held))
	{
		return HeldKind::Fence;
	}
	if (const auto* mobile = registry.TryGet<const MobileStatic>(held);
	    mobile != nullptr && Locator::infoConstants::has_value())
	{
		const auto& infos = Locator::infoConstants::value().mobileStatic;
		const auto index = static_cast<size_t>(mobile->type);
		if (index < infos.size() && animated_static::IsGateStoneKind(infos[index].mobileType))
		{
			return HeldKind::GateStone;
		}
	}
	return HeldKind::Other;
}

held_apply::TargetFacts held_apply::FactsFor(entt::entity held, entt::entity target)
{
	TargetFacts facts;
	switch (KindOfHeld(held))
	{
	case HeldKind::Tree:
	case HeldKind::DeadTree:
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Wood);
		break;
	case HeldKind::Pot:
	{
		const auto type = object_resources::GetResourceType(held);
		facts.storesHeldType = resource_stores::IsResourceStore(target, type);
		auto& registry = Locator::entitiesRegistry::value();
		facts.sameTypePot = target != entt::null && registry.Valid(target) && registry.AllOf<Pot>(target) &&
		                    object_resources::GetResourceType(target) == type;
		break;
	}
	case HeldKind::Animal:
	{
		auto& registry = Locator::entitiesRegistry::value();
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Food);
		facts.storagePit = target != entt::null && registry.Valid(target) && registry.AllOf<StoragePit>(target);
		facts.heldIndestructible = registry.AllOf<Indestructible>(held);
		break;
	}
	case HeldKind::Fence:
		facts.storesHeldType = resource_stores::IsResourceStore(target, ResourceType::Wood);
		facts.heldIndestructible = Locator::entitiesRegistry::value().AllOf<Indestructible>(held);
		break;
	case HeldKind::GateStone:
	{
		const auto& registry = Locator::entitiesRegistry::value();
		const auto* still =
		    target != entt::null && registry.Valid(target) ? registry.TryGet<const AnimatedStatic>(target) : nullptr;
		facts.gateStonePlinth = still != nullptr && still->type == AnimatedStaticInfo::GateStonePlinth;
		break;
	}
	case HeldKind::Other:
		break;
	}
	return facts;
}

bool held_apply::ValidToApplyThisToObject(entt::entity held, entt::entity target)
{
	return HeldClassValid(KindOfHeld(held), FactsFor(held, target));
}

bool held_apply::IsSacrificeAltar(entt::entity target)
{
	return Locator::entitiesRegistry::value().AllOf<WorshipTotem>(target);
}

int held_apply::ApplyThisToObject(entt::entity held, entt::entity target, const pot_resource::Dropper& dropper,
                                  const std::function<void()>& leaveHand)
{
	const auto action = HeldClassApply(KindOfHeld(held), FactsFor(held, target));
	bool done = false;
	// its ghost where the hand draws it, though it leaves the hand before it is taken
	const object_ghosts::KeepDrawnPose drawnPose(held);
	if (action == ApplyAction::GiveToStore)
	{
		// the store takes the object: out of the hand first, then its resource is taken and it goes
		if (leaveHand)
		{
			leaveHand();
		}
		done = resource_stores::DeleteObjectAndTakeResource(target, held, dropper);
	}
	else if (action == ApplyAction::MergeIntoPot)
	{
		// the held pot's resource is put down at its own position (it merges into the pots and stores around, else
		// makes a pile), then the pot goes
		auto& registry = Locator::entitiesRegistry::value();
		const auto type = object_resources::GetResourceType(held);
		const uint32_t amount = object_resources::GetResource(held, type);
		const bool poisoned = object_resources::IsPoisoned(held);
		const auto position = registry.Get<const Transform>(held).position;
		if (leaveHand)
		{
			leaveHand();
		}
		pot_resource::AddResourceToPos(position, dropper, type, amount, poisoned, false);
		object_ghosts::Add(held);
		ToBeDeleted(held);
		done = true;
	}
	else if (action == ApplyAction::LayInPlinth && Locator::animatedStaticSystem::has_value())
	{
		// the plinth takes the stone in its first empty slot (a full one takes it too, and it is lost); the stone leaves
		// the hand, a ghost of it fades and it goes
		if (leaveHand)
		{
			leaveHand();
		}
		(void)Locator::animatedStaticSystem::value().LayGateStone(target, held);
		object_ghosts::Add(held);
		ToBeDeleted(held);
		done = true;
	}
	Locator::entitiesRegistry::value().SetDirty();
	return ApplyResult(action, done);
}

entt::entity held_apply::BuildingSiteAt(glm::vec3 position)
{
	if (!Locator::mapCellsSystem::has_value())
	{
		return entt::null;
	}
	// the point through its map coordinates (x, z), as the cell lists read it
	const auto coords = map_coords::FromMetres(glm::vec2(position.x, position.z));
	if (!map_coords::InBounds(coords))
	{
		return entt::null;
	}
	const glm::vec2 point = map_coords::ToMetres(coords);
	std::vector<entt::entity> objects;
	std::vector<SiteCandidate> candidates;
	map_cells::ForEachFixed(map_coords::Cell(coords), [&](entt::entity object) {
		SiteCandidate candidate;
		candidate.multiCell = map_cells::IsMultiCellStaticClass(object);
		if (candidate.multiCell)
		{
			const auto* shape = map_cells::CollideDataOf(object);
			candidate.built = abodes::IsBuilt(object);
			candidate.repaired = abodes::IsRepaired(object);
			candidate.hasShape = shape != nullptr;
			candidate.shapeTouches = shape != nullptr && map_collide::Collide(point, k_BuildingSitePointRadius, *shape);
		}
		objects.push_back(object);
		candidates.push_back(candidate);
		return true;
	});
	const auto index = BuildingSiteAmong(candidates);
	return index ? objects[*index] : entt::null;
}

entt::entity held_apply::BuildingSiteTakingReleased(entt::entity held)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (held == entt::null || !registry.Valid(held) || !ReleaseLooksForBuildingSite(KindOfHeld(held)))
	{
		return entt::null;
	}
	const auto* transform = registry.TryGet<const Transform>(held);
	if (transform == nullptr)
	{
		return entt::null;
	}
	// the site under the object's own place, not under the hand's point; it must store the object's resource (a pot's
	// own type; a tree's, a dead tree's and a fence's is wood)
	const auto site = BuildingSiteAt(transform->position);
	const auto type = object_resources::GetResourceType(held);
	return ReleasedGoesToSite(site != entt::null, site != entt::null && resource_stores::IsResourceStore(site, type))
	           ? site
	           : entt::null;
}

int held_apply::ApplyHeldToMapCoord(entt::entity held, const pot_resource::Dropper& dropper,
                                    const std::function<void()>& leaveHand)
{
	const auto site = BuildingSiteTakingReleased(held);
	if (site == entt::null)
	{
		return 0;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// its ghost where the hand draws it, though it leaves the hand before it is taken
	const object_ghosts::KeepDrawnPose drawnPose(held);
	if (leaveHand)
	{
		leaveHand();
	}
	const bool done = resource_stores::DeleteObjectAndTakeResource(site, held, dropper);
	registry.SetDirty();
	return done ? k_ApplyConsumed : 0;
}
