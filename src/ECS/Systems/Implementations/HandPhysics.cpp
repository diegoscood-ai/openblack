/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's side of the physics system: what thrown trees, pots and wood do when they hit a store or come to rest
// (Tree/DeadTree/Pot ReactToPhysicsImpact and EndPhysics)

#define LOCATOR_IMPLEMENTATIONS

#include "HandSystem.h"
#include "HandSystemDetail.h"

#include <spdlog/spdlog.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::hand_detail;

void HandSystem::RegisterPhysicsHandlers() noexcept
{
	physics::PhysicsObjects::Handlers handlers;
	handlers.reactToImpact = [this](entt::entity entity, const physics::PhysicsObject& po) {
		auto& registry = Locator::entitiesRegistry::value();
		// Tree / DeadTree::ReactToPhysicsImpact: hitting a wood store turns it into wood (DeleteObjectAndTakeResource)
		if (registry.AnyOf<Tree, DeadTree>(entity) && po.hitBy != nullptr && registry.Valid(po.hitBy->entity) &&
		    registry.AllOf<StoragePit>(po.hitBy->entity))
		{
			physics::PhysicsObjects::RemoveObject(entity);
			DepositInStore(entity, po.hitBy->entity);
			return true;
		}
		return false;
	};
	handlers.endPhysics = [this](entt::entity entity, const physics::PhysicsObject& po) {
		auto& registry = Locator::entitiesRegistry::value();
		const auto position = registry.Get<const Transform>(entity).position;
		if (registry.AllOf<Tree>(entity))
		{
			// Tree::EndPhysics: a thrown tree never lands planted (LANDED is only set by a gentle release): DeadTree
			UpdateRoots(entity, true);
			MakeDeadTree(entity, po.body.velocity, false);
			return entity;
		}
		if (const auto type = PotInfoOf(entity); (type == PotInfo::HandWood || type == PotInfo::HandFood) && IsLand(position))
		{
			// Pot::EndPhysics: a hand pot out of the water becomes a pile (AddResourceToPos)
			PutDownHandPot(entity);
			return registry.Valid(entity) ? entity : entt::entity {entt::null};
		}
		return entity;
	};
	handlers.moved = [this](entt::entity entity) {
		if (Locator::entitiesRegistry::value().AllOf<Tree>(entity))
		{
			UpdateRoots(entity);
		}
	};
	physics::PhysicsObjects::SetHandlers(std::move(handlers));
	physics::PhysicsObjects::LoadConstants();
}
