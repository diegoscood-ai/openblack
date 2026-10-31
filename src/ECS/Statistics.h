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

#include "ECS/StatisticsRules.h"
#include "Enums.h"

/// The players' statistics as the game keeps them each turn, over the game statistics of game_stats: the population's
/// extremes, the population and influence histories and the creature's size the first time it is seen; and the
/// figures of the temple's world scroll.
namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::player_statistics
{

/// Each game turn, after the players' influence power: for each player, the men and women of its towns go into its
/// extremes and its population history, its influence power into its influence history, and its creature's size into
/// its statistics the first time the creature is seen
void ProcessTurn(uint32_t turn);

/// The game's start of each player's statistics, once, on the first land after its map script: its alignment and its
/// creature's, the time it started and the number of towns
void Start();

/// Everything the Game Statistics box shows now, for the first four players. As in the game, when the box takes the
/// figures the lines of code take one more step, which draws a local random number
[[nodiscard]] statistics::BoxFigures Box();

/// A creature ate a thing of an object type (a person, an animal or a creature counts as killed), ate a magic mushroom or
/// a toadstool, or pooed
void CountCreatureMeal(Registry& registry, entt::entity creature, uint32_t type);
void CountCreatureMushroom(Registry& registry, entt::entity creature);
void CountCreaturePoo(Registry& registry, entt::entity creature);
/// The end of a meal the creature eats: only while the thing eaten is still there to be used up, the kill by its type
/// and, for a mushroom, the mushrooms eaten; a meal already gone counts nothing
void CountCreatureMealEnd(Registry& registry, entt::entity creature, bool mealAvailable, uint32_t type, bool mushroom);
/// What the player's primary creature counts of its life; its fights and the fights it won are its fight record's.
/// Nothing without a creature
[[nodiscard]] statistics::CreatureTallies CreatureTalliesOf(PlayerNames player);

/// What the temple's world scroll tells a player: the world's people, the player's towns and people, its births,
/// deaths and sacrifices
[[nodiscard]] statistics::WorldFigures World(PlayerNames player);

} // namespace openblack::ecs::player_statistics
