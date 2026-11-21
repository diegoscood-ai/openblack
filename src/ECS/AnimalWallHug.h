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

#include <span>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "ECS/AnimalAIDetail.h"

/// The circle hug of the animals' walks: LINEAR -> ORBIT -> EXIT_CIRCLE round the collide circles of the fixed objects
/// and of the water cells (docs/wiki/beings/wall-hugging.md#walking-round-obstacles-mobilewallhug). Only the walks set up with
/// SetupMoveToWithHug use it: GOTO_FOOD_REACTION and the re-flee of FLEEING_FROM_OBJECT. Every other animal walk is
/// STEP_THROUGH (ECS/AnimalAI.cpp MoveTo).
namespace openblack::ecs::animal_ai::detail
{

/// The move states of the hug, the walkers' shared ones (wall_hug::WalkState)
constexpr uint8_t k_MoveLinear = MoveStateOf(wall_hug::WalkState::Linear);
constexpr uint8_t k_MoveLinearCw = MoveStateOf(wall_hug::WalkState::LinearClockwise);
constexpr uint8_t k_MoveLinearCcw = MoveStateOf(wall_hug::WalkState::LinearAnticlockwise);
constexpr uint8_t k_MoveOrbitCw = MoveStateOf(wall_hug::WalkState::OrbitClockwise);
constexpr uint8_t k_MoveOrbitCcw = MoveStateOf(wall_hug::WalkState::OrbitAnticlockwise);
constexpr uint8_t k_MoveExitCircleCcw = MoveStateOf(wall_hug::WalkState::ExitAnticlockwise);
constexpr uint8_t k_MoveExitCircleCw = MoveStateOf(wall_hug::WalkState::ExitClockwise);

bool IsHugMoveState(uint8_t state);

/// Sets the current and destination states (info.moveState, final), then sets up the move to `p` as LINEAR: the
/// obstacle sweep and LINEAR
void SetupMoveToWithHug(Context& ctx, glm::vec2 p, AnimalState final);

/// The LINEAR, ORBIT_CW / CCW and EXIT_CIRCLE handlers: 6 / 7 when it stepped (same / new map cell), 1 when it only
/// changed state
int HugMoveTo(Context& ctx);

/// A thing of many cells left the map: an animal going round one of the things it overlaps (`overlapping`, the thing
/// itself among them) leaves its orbit at its next chance, whatever its distance to its goal. Water is never marked.
/// True when the animal was marked
bool MarkLeaveAtOnce(components::AnimalBrain& brain, std::span<const entt::entity> overlapping);

/// The circle the animal hugs went (its thing left the map, or its collide data is made again): what the walk does at
/// once, as the villagers' (wall_hug::OnCircleGone)
void OnHuggedCircleGone(Context& ctx);

} // namespace openblack::ecs::animal_ai::detail
