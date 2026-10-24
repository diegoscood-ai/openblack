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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

// The missionary disciple: put down by another town's building it moves into the closest abode, and a missionary
// control (components::MissionaryControl) in that town's list preaches there for its own player through a
// ReactToMissionary reaction, until the missionary goes, is no longer one, or the reaction ends.

namespace openblack::ecs::components
{
struct LivingAction;
}

namespace openblack::ecs::villager
{
/// 226 MISSIONARY_DISCIPLE: the closest abode of any town (abode_queries::FindClosestAbode, no tribe, no flags): its
/// town gets AddMissionary(me, my own town's player), ForceMoveVillagerToAbode, 234 GO_HOME_AND_CHANGE; no abode -> 163.
/// (guard) no own town (the original reads its player unchecked) -> 163. Always 1
uint32_t MissionaryDisciple(components::LivingAction& action);
/// A missionary control at the map's origin, its reaction 39 of `player` made (and spread from there), at the head of
/// the town's list
void AddMissionary(entt::entity town, entt::entity villager, PlayerNames player);
/// The control's turn: no missionary or reaction -> deleted; the missionary unavailable or no longer a missionary -> its
/// reaction taken off and the control deleted; its reaction gone -> deleted; else it follows the missionary
void ProcessMissionary(entt::entity control);
/// The control goes; a missionary still preaching is no disciple any more
void DeleteMissionary(entt::entity control);
/// Step 18 of the town's turn: each control of the list, newest first, has its turn; those no longer available leave
/// the list
void ProcessMissionaries(entt::entity town);
/// Every control of the town's list deleted, the list emptied (a town changing hands)
void DeleteAllMissionaries(entt::entity town);
/// How much a missionary impresses one villager: the land's balance by the missionary's value by the reaction's
/// multiplier, then the boredom by the distance's share (gutils::DistanceChangeToBelief) by that, in the original's order
[[nodiscard]] float MissionaryImpressiveValue(float landBalance, float missionaryValue, float multiplier, float beliefShare,
                                              float boredom);
} // namespace openblack::ecs::villager
