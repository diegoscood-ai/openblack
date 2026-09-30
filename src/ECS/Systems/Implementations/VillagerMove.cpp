/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#include "VillagerMove.h"

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

void ecs::villager::SetupMoveToWithHug(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (action == nullptr || wallHug == nullptr)
	{
		return;
	}
	wallHug->goal = goal;
	wallHug->step = glm::vec2(0.0f);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Remove<WallHugObjectReference>(villager);
	registry.Assign<MoveStateLinearTag>(villager);
	// TOP first (its SetState clears FINAL), then FINAL: 0x752440
	auto& system = Locator::livingActionSystem::value();
	system.VillagerSetState(*action, LivingAction::Index::Top, VillagerStates::MoveToPos, true);
	system.VillagerSetState(*action, LivingAction::Index::Final, final, true);
}
