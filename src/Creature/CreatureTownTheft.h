/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Creature/CreatureIdleMind.h"
#include "Enums.h"

/// The thefts that a creature copies from its player: it takes a stolen thing to a town of its player, or it takes food
/// or wood from the store of a town of another player to a store of its own player. The rules and the agendas here are
/// pure, free of the game.
namespace openblack::creature_town_theft
{
using creature_mind::Step;

/// A storage pit belongs to the player of the creature when it is built and it has the same player as the creature. A
/// thing of no player has the same player as a creature of no player.
[[nodiscard]] bool IsOwnStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer);
/// A storage pit belongs to another player when it is built and its player is not the player of the creature. A pit of
/// a neutral town, or of no town, also belongs to another player.
[[nodiscard]] bool IsOthersStoragePit(bool built, std::optional<uint32_t> pitPlayer, std::optional<uint32_t> creaturePlayer);
/// A field belongs to another player when its player is not the player of the creature. The field can be empty.
[[nodiscard]] bool IsOthersField(std::optional<uint32_t> fieldPlayer, std::optional<uint32_t> creaturePlayer);

/// The nearest of the towns of the player of the creature, from the distance of each town to the creature in metres.
/// Of two towns at the same distance, the first is the nearest. None when the player has no town.
[[nodiscard]] std::optional<size_t> NearestTown(std::span<const float> metres);

/// How near to the storage pit of the town the creature goes to put down a stolen thing: three times its height
inline constexpr float k_PutInTownHeights = 3.0f;

/// The creature steals a thing and takes it to a town of its player. When its hand is empty, it picks up the thing and
/// looks at the thing as it goes to it. It goes near the storage pit of the nearest town of its player, to within
/// k_PutInTownHeights of its height, looking at the pit as it goes, and puts the thing down. The agenda has no main step.
[[nodiscard]] std::vector<Step> StealAndPutInTown(std::optional<uint32_t> pickUp, uint32_t pit, float height);

/// The handful the creature takes from a store it steals from: wheat for food, logs for wood
[[nodiscard]] PotInfo StolenHandful(bool wood);

/// The creature steals food or wood from the storage pit of another player. It goes right up to that pit, looking at
/// it as it goes, and then gives the handful to its own store as a gift of food from a field does
/// (creature_town_help::GiveHandfulToStore): it turns to face the pit, takes the handful, goes to where it can throw
/// into its own store and throws it in, the throw being the main step. The pit it steals from loses nothing.
[[nodiscard]] std::vector<Step> StealFromStore(uint32_t from, glm::vec2 fromPoint, uint32_t into, bool wood, float height);
} // namespace openblack::creature_town_theft
