/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMoveToObject.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GameDistance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingTurn.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/WalkGoal.h"
#include "ECS/WalkStateOf.h"
#include "Locator.h"

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
/// A walk's target's speed is at least this share of what the walker follows
constexpr float k_FollowSpeedShare = 1.25f;
/// MoveToObjectPos's results
constexpr uint32_t k_Arrived = 10;
constexpr uint32_t k_TargetGone = 38;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// What the thing moves at, in metres a turn: a walker's speed, nothing for a thing that does not walk
float TargetSpeedInMetres(entt::entity object)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(object);
	return wallHug != nullptr ? wallHug->speed : 0.0f;
}

void RemoveMoveStates(entt::entity villager)
{
	ecs::wall_hug::DropWalkTags<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                            MoveStateFinalStepTag, MoveStateArrivedTag>(Entities(), villager);
}

/// Off the circle-hug lists, as a walk's set-up does: an entry distance too long for the hug's own field goes back to 0
void ForgetLongEntryDistance(entt::entity villager)
{
	if (auto* wallHug = Entities().TryGet<WallHug>(villager); wallHug != nullptr && wallHug->entryDistance >= 0xFFFF)
	{
		wallHug->entryDistance = 0;
	}
}
} // namespace

void SetupMobileMoveToObject(entt::entity villager, entt::entity object)
{
	auto& registry = Entities();
	if (object == entt::null)
	{
		return;
	}
	auto* v = registry.TryGet<Villager>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (v == nullptr || wallHug == nullptr || transform == nullptr)
	{
		return;
	}
	v->walkTarget = object;
	const auto goal = map_coords::ToMetres(object::GetWorkingPos(object, villager));
	walk_goal::Set(villager, wallHug->goal, goal);
	InitStepsXZ(*transform, *wallHug);
	ForgetLongEntryDistance(villager);
	RemoveMoveStates(villager);
	if (AreWeThere(villager, goal, 0.0f))
	{
		registry.Assign<MoveStateArrivedTag>(villager, MoveStateClockwise::Undefined, goal);
		return;
	}
	wallHug->turnsUntilStepRebuild = 1;
	registry.Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform->position));
}

bool SetupMoveToObject(entt::entity villager, entt::entity object, VillagerStates final)
{
	SetupMobileMoveToObject(villager, object);
	return SetCurrentAndDestinationState(villager, VillagerStates::MoveToObject, final) == 1;
}

bool StartMoveToObject(entt::entity villager, entt::entity object, VillagerStates final)
{
	static_cast<void>(SetupMoveToObject(villager, object, final));
	return true;
}

uint32_t MoveToObjectPos(entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (v == nullptr || wallHug == nullptr)
	{
		return k_TargetGone;
	}
	const auto target = v->walkTarget;
	if (target == entt::null || !ecs::IsAvailable(target))
	{
		return k_TargetGone;
	}
	const auto pos = object::MapCoordsOf(target);
	float speed = wallHug->speed;
	if (game_math::GetDistanceInMetres(object::MapCoordsOf(villager), pos) < speed)
	{
		return k_Arrived;
	}
	// a villager is no animal: it keeps up with what it follows
	const float targetSpeed = TargetSpeedInMetres(target) * k_FollowSpeedShare;
	if (targetSpeed > speed)
	{
		speed = targetSpeed;
		wallHug->speed = speed;
	}
	walk_goal::Set(villager, wallHug->goal, map_coords::ToMetres(pos));
	if ((v->flags & Villager::k_FlagFootball) != 0)
	{
		if (auto* transform = registry.TryGet<Transform>(villager); transform != nullptr)
		{
			InitStepsXZ(*transform, *wallHug);
		}
	}
	living_turn::MoveToStep(villager);
	return living_turn::WallHugMoveToResult(villager);
}

uint32_t MoveToObject(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto result = MoveToObjectPos(villager);
	if (result == k_Arrived)
	{
		// the arrival is taken: the next walk or the final state starts afresh, as MOVE_TO_POS's
		ecs::wall_hug::DropWalkTags<MoveStateFinalStepTag, MoveStateArrivedTag>(registry, villager);
		SetTopStateToFinal(villager);
		if (auto* v = registry.TryGet<Villager>(villager); v != nullptr)
		{
			v->walkTarget = entt::null;
		}
	}
	else if (result == k_TargetGone)
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
		if (auto* v = registry.TryGet<Villager>(villager); v != nullptr)
		{
			v->walkTarget = entt::null;
		}
		return 1;
	}
	return result;
}

} // namespace openblack::ecs::villager
