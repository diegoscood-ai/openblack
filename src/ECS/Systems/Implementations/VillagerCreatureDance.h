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

#include <vector>

#include <entt/entity/fwd.hpp>

#include "Enums.h"
#include "VillagerDance.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers that a creature calls to dance with it. The creature controls them until it tells them to dance. Then
/// they dance and worship the creature, until its dance stops.
namespace openblack::ecs::villager_creature_dance
{

/// Whether a creature can call a villager to its dance: not in a dance, not a child, not a disciple, and its life more
/// than the life at which it goes home hurt
[[nodiscard]] bool AvailableToBeSummoned(entt::entity villager);
/// The villagers of a town's buildings that a creature can call, in the order that it calls them: the last villager of
/// the last building first
[[nodiscard]] std::vector<entt::entity> SummonOrder(entt::entity town);
/// The villager joins the creature's dance and comes under the creature's control, at its walking speed. False when no
/// group of the dance takes it.
bool StartWorshippingCreature(entt::entity villager, entt::entity dance);
/// The creature sends the villager to its place in the dance, at a run. False for a villager in no group of a dance.
bool SendToPlace(entt::entity villager, entt::entity dance);
/// The creature tells the villager to dance and worship, when the villager dances in a group of a dance
void TellToDance(entt::entity villager);
/// A villager that worships in a dance gives its town belief in the player of the creature that the dance is for, each
/// turn: the town's share of the population for unmodified belief, times the creature's impressive size and the
/// dance's impressiveness. A dance for anything other than a creature gives no belief.
void WorshipCreature(entt::entity villager, entt::entity owner, DanceInfo type);

/// 204 CONTROLLED_BY_CREATURE: a villager that stands right at the middle of the dance walks to its place. Always 1
uint32_t ControlledByCreature(components::LivingAction& action);
/// 204's exit: the creature's control ends only when the villager goes into the hand, flies or dies (then it leaves the
/// creature's list and its dance; 1), or when it goes to dance, to worship or to decide what to do (1); every other
/// state is refused (0)
uint32_t ExitControlledByCreature(components::LivingAction& action, VillagerStates next);
/// 90 WORSHIPPING_CREATURE: dancing and worshipping the creature while the dance is on (its walk to its place by
/// `walk`), and going home once it stops
uint32_t WorshippingCreature(components::LivingAction& action, const villager_dance::WalkStep& walk);
/// 66 RESTART_WORSHIPPING_CREATURE: back to the creature's dance after a reaction, it dances the turning move (9),
/// whatever its group does. A villager in no dance goes home
uint32_t RestartWorshippingCreature(components::LivingAction& action, const villager_dance::WalkStep& walk);

} // namespace openblack::ecs::villager_creature_dance
