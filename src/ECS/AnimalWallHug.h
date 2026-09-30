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

#include <glm/vec2.hpp>

#include "ECS/AnimalAIDetail.h"

/// The circle hug of MobileWallHug::MoveTo (0x60AF20) for the animals: LINEAR -> ORBIT -> EXIT_CIRCLE round the collide
/// circles of the fixed objects and of the water cells (research dev\tmp_dis\animals\wallhug.md and wallhug_circle.md).
/// Only the walks set up with Living::SetupMoveToWithHug (0x5F2890) use it: GOTO_FOOD_REACTION and the re-flee of
/// FLEEING_FROM_OBJECT. Every other animal walk is STEP_THROUGH (ECS/AnimalAI.cpp MoveTo).
namespace openblack::ecs::animal_ai::detail
{

/// MOVE_TO_STATES (MobileWallHug +0x5E) of the hug
constexpr uint8_t k_MoveLinear = 0xC;
constexpr uint8_t k_MoveLinearCw = 0xD;
constexpr uint8_t k_MoveLinearCcw = 0xE;
constexpr uint8_t k_MoveOrbitCw = 0xF;
constexpr uint8_t k_MoveOrbitCcw = 0x10;
constexpr uint8_t k_MoveExitCircleCcw = 0x11;
constexpr uint8_t k_MoveExitCircleCw = 0x12;

bool IsHugMoveState(uint8_t state);

/// Living::SetupMoveToWithHug (0x5F2890): SetCurrentAndDestinationState(info.moveState, final), then
/// MobileWallHug::SetupMobileMoveToPos(p, LINEAR) (0x60ABC0): the obstacle sweep and LINEAR
void SetupMoveToWithHug(Context& ctx, glm::vec2 p, AnimalState final);

/// MobileWallHug::MoveTo's LINEAR (0x60B095), ORBIT_CW / CCW (0x60B0E4 / 0x60B40E) and EXIT_CIRCLE (0x60B78F) handlers:
/// 6 / 7 when it stepped (same / new map cell), 1 when it only changed state
int HugMoveTo(Context& ctx);

} // namespace openblack::ecs::animal_ai::detail
