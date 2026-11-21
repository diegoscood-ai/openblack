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

#include "3D/AllMeshes.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// Villagers watching a creature fight (ECS/FightWatchRules.h for the rules). Every arena, a land's or one a fight made,
/// starts a fight reaction as it is made, which villagers heed only while a fight is on in it: each one that takes it up
/// walks to its place on a ring round the arena and faces it, cheering, despairing or chatting. A villager reacting to a
/// creature that goes into a fight leaves it to take up its arena's reaction. Wiki:
/// docs/wiki/beings/villager-reactions.md, the villager type table's ReactToFight row and states 143 and 144.
namespace openblack::ecs::villager_fight_watch
{
/// The type table's setup: the arena becomes the villager's reaction object, and it starts at state 143
void SetupReactToFight(entt::entity villager, entt::entity arena, uint32_t reaction);
/// The type table's priority: the table's, while a fight is on in the arena that started the reaction; else 0
[[nodiscard]] uint8_t ReactToFightPriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// A villager reacting to a creature in a fight takes up the reaction its arena started, if it finds one
void WatchCreaturesFight(entt::entity villager, entt::entity arena);
/// State 143: its place on the ring round the arena, by how many followers of the reaction joined after it, and the
/// walk there into state 144; with no arena to watch it stops reacting
uint32_t InitialiseFightReaction(components::LivingAction& action);
/// State 144: it faces the arena, quickly
uint32_t PerformFightReaction(components::LivingAction& action);
/// State 144's clip: standing with no fight to watch, else the clip WatchFightClip picks
[[nodiscard]] AnimId WatchFightAnimation(entt::entity villager);
} // namespace openblack::ecs::villager_fight_watch
