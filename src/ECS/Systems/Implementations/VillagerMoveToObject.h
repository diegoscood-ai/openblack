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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

/// A villager's walk to a thing that may move (state 2 MOVE_TO_OBJECT): set up towards the thing's working point by
/// the villager, then each turn after the thing itself, until within the walker's speed of it
/// (docs/wiki/beings/villager-states.md#walking-to-a-thing).
namespace openblack::ecs::villager
{

/// The walk set up: the thing kept as the walk's target, the goal its working point by the villager, the step aimed;
/// there already, the walk has arrived, else it goes straight, its step aimed again on its first turn. Nothing for no
/// thing
void SetupMobileMoveToObject(entt::entity villager, entt::entity object);

/// SetupMobileMoveToObject, then MOVE_TO_OBJECT now and `final` once there (SetCurrentAndDestinationState). Whether
/// the states were taken
bool SetupMoveToObject(entt::entity villager, entt::entity object, VillagerStates final);

/// The same, always true (the game's own start of such a walk)
bool StartMoveToObject(entt::entity villager, entt::entity object, VillagerStates final);

/// One turn of the walk to the thing: a thing gone or no longer available ends it (38); within the walker's speed of
/// it, arrived (10); else the walker goes at least as fast as 1.25 times the thing, the goal is the thing's place (a
/// villager playing football aims its step again every turn) and it takes its step. The walk's result
[[nodiscard]] uint32_t MoveToObjectPos(entt::entity villager);

/// State 2 MOVE_TO_OBJECT: MoveToObjectPos; arrived: the final state and no target; the thing gone: deciding afresh
/// (163) and no target, 1; else the walk's result
uint32_t MoveToObject(components::LivingAction& action);

} // namespace openblack::ecs::villager
