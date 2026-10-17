/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/WallHug.h"
#include "Enums.h"

// The villager's walk to a point and the state helpers the scripts use, as the original does them
// (docs/wiki/scripts/script-commands.md#script-commands-that-move-things-move_game_thing-033-and-friends): the walk
// (SetupMoveToPos, AreWeThere), the previous state, whether it is available and in the map, and WAIT_FOR_ANIMATION (23) between
// two clips. What the scripts themselves do to a villager is villager_script's (ECS/Systems/Implementations/VillagerScript.h).

namespace openblack::ecs::components
{
struct Transform;
struct WallHug;
} // namespace openblack::ecs::components

namespace openblack::ecs::villager
{
/// Sets the villager's Y angle as openblack keeps it: WallHug::yAngle (the 3D angle, VillagerCore's convention) and
/// the drawn rotation the pathfinding gives the transform, AngleY(angle + 90 degrees), the angle the world matrix
/// uses. (approximate) no u16 game angle is stored: VillagerCore derives it from yAngle when it reads it (rounded to
/// 2048ths). Shared by PathfindingSystem's InitializeStep (openblack's orbit) and ECS/SuperVillager; VillagerCore's
/// SetGameAngle (the game angle in, then FaceAngle) is a second place that sets the object's Y angle, kept
void SetYAngle(components::Transform& transform, components::WallHug& wallHug, float angle);
/// For villagers: a = the u16 game angle from Pos to GetDestPos (integer arctangent), SetTowardsAngle(a)
/// (SetGameAngle, no turn limit), then the step ((COS / SIN[a] x (speed >> 4)) >> 12, gutils::StepFromAngle) in
/// MapCoords units (WallHug::step), and the same in metres (WallHug::stepMetres). The one copy: SetupMobileMoveToPos
/// and PathfindingSystem's
/// InitializeStepToGoal call it. (approximate) Pos and the goal are metres made MapCoords here
void InitStepsXZ(components::Transform& transform, components::WallHug& wallHug);
/// The straight line's look ahead: the first obstacle along the walker's step from where it stands, through the
/// pathfinding system (none without it)
void SweepAhead(entt::entity villager);
/// The walk becomes LINEAR (no side round circles yet) and looks ahead at once, with the step it has: the goal and the
/// step stay as they are
void SetLinearAndSweep(entt::entity villager);
/// A walk to `goal` that starts in `state`, LINEAR or STEP_THROUGH: the goal, InitStepsXZ, then AreWeThere(0) -> ARRIVED
/// (the hug left as it is); else LINEAR looks ahead now (SetLinearAndSweep), and STEP_THROUGH aims its step again on
/// its first turn. Not the script's walk (SetupMoveToPos's, always STEP_THROUGH, which also resets the hug)
void SetupMobileMoveToPos(entt::entity villager, const glm::vec2& goal, components::MoveState state);
/// (a, b): the entry functions of the state rows (VillagerOriginalFns.h)
/// of both states are the same (EnterInScript, EnterPlayAnim, EnterBuilding)
[[nodiscard]] bool IsStateEntryFunctionSameAs(VillagerStates a, VillagerStates b);
/// AreWeThere(pos, r): walk_arrival::AreWeThere on the villager's position and `pos`, each made a MapCoords from the
/// metres openblack keeps (map_coords::FromMetres), with its whole speed (WholeSpeed) and `r` in map units. The one
/// copy: every arrival test of a villager walk calls it
[[nodiscard]] bool AreWeThere(entt::entity villager, const glm::vec2& pos, float r);
/// The walk's goal (openblack's WallHug::goal)
[[nodiscard]] std::optional<glm::vec2> GetDestPos(entt::entity villager);
/// AreWeThere(GetDestPos(), r)
[[nodiscard]] bool AreWeThereAtDestination(entt::entity villager, float r);
/// (pos, final): SetCurrentAndDestinationState(the living info's moveState, final) and,
/// only if it returns 1, SetupMobileMoveToPos: a STEP_THROUGH walk (no obstacle hugging), or
/// ARRIVED if it is there already. Returns 1 if the walk was set up, else 0.
uint32_t SetupMoveToPos(entt::entity villager, const glm::vec2& goal, VillagerStates final);
/// The final state goes to PREVIOUS, unless it is a passing one
/// (not stored as previous, or reactive, in the state info), which keeps what was stored
void StorePreviousState(entt::entity villager);
/// Not being deleted and the final state is not 14 DYING
[[nodiscard]] bool IsAvailable(entt::entity villager);
/// The object's in-map flag: map_cells::IsObjectInMap. A villager leaves the map cells in the hand, in flight and in a
/// storm, and enters them again where it lands
[[nodiscard]] bool IsObjectInMap(entt::entity villager);
/// (state, unused): CallExitStateFunction(state) and, if 1,
/// CallEntryStateFunction(23 WAIT_FOR_ANIMATION, state); the clip is left as it is
void PlayAnimThenSetState(entt::entity villager, VillagerStates state);

/// State 23 WAIT_FOR_ANIMATION
uint32_t WaitForAnimation(components::LivingAction& action);
} // namespace openblack::ecs::villager
