/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScript.h"

#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsAngle.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/PathfindingSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateFunctions.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/WalkArrival.h"
#include "ECS/WalkGoal.h"
#include "InfoConstants.h"
#include "Locator.h"

// The villager's walk to a point and the state helpers the scripts use (VillagerScript.h;
// docs/wiki/scripts/script-commands.md#script-commands-that-move-things-move_game_thing-033-and-friends).

namespace openblack::ecs::villager
{
using namespace components;
using state_info::StateInfo;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

/// Off the circle-hug lists: an entry distance too long for the hug's own field (0xFFFF or more, which the original
/// keeps apart from it) goes back to 0; the circle held is kept
void ForgetLongEntryDistance(entt::entity villager)
{
	if (auto* held = Entities().TryGet<WallHugObjectReference>(villager); held != nullptr && held->entryDistance >= 0xFFFF)
	{
		held->entryDistance = 0;
	}
}

/// (pos): the goal = pos, InitStepsXZ, off the circle-hug lists, then AreWeThere(0) -> ARRIVED, the circle held kept;
/// else the circle-hug info is reset, the step is due to be aimed again on the first turn and the walk is STEP_THROUGH
void SetupMobileMoveToPos(entt::entity villager, const glm::vec2& goal)
{
	auto& registry = Entities();
	auto& wallHug = registry.Get<WallHug>(villager);
	auto& transform = registry.Get<Transform>(villager);
	walk_goal::Set(villager, wallHug.goal, goal);
	InitStepsXZ(transform, wallHug);
	ForgetLongEntryDistance(villager);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	if (AreWeThere(villager, goal, 0.0f))
	{
		// ARRIVED: PathfindingSystem puts it on the goal and VillagerMoveToPos sees ARRIVED
		registry.Assign<MoveStateArrivedTag>(villager, MoveStateClockwise::Undefined, goal);
		return;
	}
	registry.Remove<WallHugObjectReference>(villager);
	wallHug.turnsUntilStepRebuild = 1;
	registry.Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform.position));
}

} // namespace

bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b)
{
	// The entry functions of both states, from the original state table (an empty slot in the first -> the same at
	// once)
	const auto entryOf = [](VillagerStates s) {
		const auto i = static_cast<size_t>(static_cast<uint8_t>(s));
		return i < k_StateFunctionIds.size() ? k_StateFunctionIds.at(i).entry : 0u;
	};
	return entryOf(a) == entryOf(b);
}

bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r)
{
	const auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(villager);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	if (transform == nullptr || wallHug == nullptr)
	{
		return false;
	}
	// dx = Pos.x - pos.x, dz = Pos.z - pos.z in whole units, R = (float) speed + r; dx^2 + dz^2 < R^2 -> 1
	// (R^2 <= d^2 -> 0)
	return walk_arrival::AreWeThere(map_coords::FromMetres(glm::xz(transform->position)), map_coords::FromMetres(pos),
	                                WholeSpeed(*wallHug), r);
}

std::optional<glm::vec2> GetDestPos(entt::entity villager)
{
	const auto* wallHug = Entities().TryGet<const WallHug>(villager);
	if (wallHug == nullptr)
	{
		return std::nullopt;
	}
	return wallHug->goal;
}

bool AreWeThereAtDestination(entt::entity villager, float r)
{
	// GetDestPos, then AreWeThere(dest, r)
	const auto dest = GetDestPos(villager);
	return dest.has_value() && AreWeThere(villager, *dest, r);
}

uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final)
{
	auto& registry = Entities();
	if (!registry.AllOf<LivingAction, WallHug, Transform>(villager))
	{
		return 0;
	}
	// The living info's moveState (1 MOVE_TO_POS in all the villager rows of info.dat);
	// 3 MOVE_ON_STRUCTURE instead when the moving-on-structure flag is set, a flag only
	// MOVE_ON_STRUCTURE sets (state 3, not ported in openblack: never set here)
	const auto moveState = static_cast<VillagerStates>(static_cast<uint8_t>(InfoOf(villager).moveState));
	// SetCurrentAndDestinationState(moveState, final); not 1 -> 0
	if (SetCurrentAndDestinationState(villager, moveState, final) != 1)
	{
		return 0;
	}
	// SetupMobileMoveToPos(pos) (the one without a hug state), then 1
	SetupMobileMoveToPos(villager, goal);
	return 1;
}

void StorePreviousState(entt::entity villager)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// GetFinalState; a passing one (never kept in PREVIOUS, or a reaction) keeps what
	// PREVIOUS had; a raw SetState, no town modifiers
	const auto final = GetFinalState(villager);
	const auto& info = StateInfo(final);
	auto stored = final;
	if (state_info::NotStoredAsPrevious(info) || state_info::IsReactive(info))
	{
		stored = GetState(villager, Index::Previous);
	}
	action->states.at(static_cast<size_t>(Index::Previous)) = static_cast<uint8_t>(stored);
}

bool IsAvailable(entt::entity villager)
{
	// Being deleted (ecs::IsAvailable) -> 0; GetFinalState == DYING -> 0; else 1
	return ecs::IsAvailable(villager) && ActionOf(villager) != nullptr && GetFinalState(villager) != VillagerStates::Dying;
}

bool IsObjectInMap(entt::entity villager)
{
	// the object's own in-map flag: out while the hand holds it, while it flies and while a storm carries it
	return map_cells::IsObjectInMap(villager);
}

void PlayAnimThenSetState(entt::entity villager, VillagerStates state)
{
	// CallExitStateFunction(state); 0 -> nothing
	if (CallExitStateFunction(villager, state) == 0)
	{
		return;
	}
	// CallEntryStateFunction(23, state): TOP 23 WAIT_FOR_ANIMATION, FINAL state; no clip
	CallEntryStateFunction(villager, VillagerStates::WaitForAnimation, state);
}

uint32_t WaitForAnimation(LivingAction& action)
{
	const auto villager = EntityOf(action);
	// IsReadyForNewAnimation -> SetTopStateToFinal and 0; else 1
	if (VillagerAnimationDone(villager, action.turnsSinceStateChange))
	{
		SetTopStateToFinal(villager);
		return 0;
	}
	return 1;
}

uint32_t ScriptInCrowd(LivingAction& action)
{
	if (Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		villager_animate::SetStateAnim(EntityOf(action));
		action.turnsSinceStateChange = 0;
	}
	return 1;
}

void SetYAngle(Transform& transform, WallHug& wallHug, float angle)
{
	transform.rotation = affine::AngleY(angle + glm::radians(90.0f)); // drawn a quarter turn on from the 3D angle
	wallHug.yAngle = angle;
}

void InitStepsXZ(Transform& transform, WallHug& wallHug)
{
	// GetAngleFromXZ(Pos, GetDestPos), each one a MapCoords first
	// ((approximate) from the metres openblack keeps)
	const uint16_t angle = gutils::GetAngleFromXZ(glm::xz(transform.position), wallHug.goal);
	// SetTowardsAngle, for a villager SetGameAngle
	// (no turn limit): the game angle = a, then the Y angle from ConvertGameAngleTo3D(a)
	SetGameAngle(transform, wallHug, angle);
	// The step x = (COS[angle] x (speed >> 4)) >> 12, z the same with SIN, y = 0
	// (gutils::StepFromAngle); MapCoords units a turn, and the same in metres for the walk's readers
	const auto step = gutils::StepFromAngle(angle, WholeSpeed(wallHug));
	wallHug.step = step;
	wallHug.stepMetres = glm::vec2(map_coords::ToMetres(step.x), map_coords::ToMetres(step.y));
}

void SweepAhead(entt::entity villager)
{
	// (openblack, guard) the unit tests run state functions without the systems
	if (Locator::pathfindingSystem::has_value())
	{
		Locator::pathfindingSystem::value().SweepAhead(villager);
	}
}

void SetLinearAndSweep(entt::entity villager)
{
	auto& registry = Entities();
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (transform == nullptr || !registry.AllOf<WallHug>(villager))
	{
		return;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	registry.Assign<MoveStateLinearTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform->position));
	SweepAhead(villager);
}

void SetupMobileMoveToPos(entt::entity villager, const glm::vec2& goal, MoveState state)
{
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<WallHug>(villager);
	if (wallHug == nullptr)
	{
		return;
	}
	walk_goal::Set(villager, wallHug->goal, goal);
	ForgetLongEntryDistance(villager);
	auto* transform = registry.TryGet<Transform>(villager);
	if (transform == nullptr)
	{
		// (openblack) a walker with no position (none in the game, only in the unit tests): the state only
		registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
		registry.Assign<MoveStateLinearTag>(villager);
		return;
	}
	// InitStepsXZ (the circle-hug lists above); then AreWeThere(0) -> ARRIVED, the hug left as it is
	InitStepsXZ(*transform, *wallHug);
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	if (AreWeThere(villager, goal, 0.0f))
	{
		registry.Assign<MoveStateArrivedTag>(villager, MoveStateClockwise::Undefined, goal);
		return;
	}
	if (state == MoveState::StepThrough)
	{
		// the step is aimed again on the first turn
		wallHug->turnsUntilStepRebuild = 1;
		registry.Assign<MoveStateStepThroughTag>(villager, MoveStateClockwise::Undefined, glm::xz(transform->position));
		return;
	}
	// LINEAR: the look ahead along the step, now
	SetLinearAndSweep(villager);
}

} // namespace openblack::ecs::villager
