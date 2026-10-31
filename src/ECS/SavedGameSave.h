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

#include <array>
#include <optional>
#include <vector>

#include <SavedRecords.h>

#include "Common/GameRandom.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/Systems/SnowSystemInterface.h"

// What a save writes of the game object's own values: the inverse of saved_game_load. The values openblack keeps
// are taken from its services and put into the game object's record where the game's save writes them; every other
// value stays as the record has it. See docs/wiki/engine/saved-games.md#writing-the-world-file.
namespace openblack::ecs::saved_game_save
{

/// The game object's values openblack keeps, each group none when its service is not there (the record's then stays)
struct GameValues
{
	std::optional<uint32_t> landNumber;
	std::optional<GameRandomSeeds> seeds;
	std::optional<uint32_t> gameTurn;
	/// The creation number the next object takes
	std::optional<uint32_t> createdObjects;
	/// The player's and the towns' influence multipliers
	std::optional<std::array<float, 2>> influenceMultipliers;
	/// The time of day, its cycle as the game keeps it (the day's length, the change, the night) and its speed
	struct Clock
	{
		float visualTime {0.0f};
		std::array<float, 3> cycle {};
		float scale {0.0f};
	};
	std::optional<Clock> clock;
	std::optional<systems::SavedSnow> snow;
	/// The did-you-know texts read, by category
	std::optional<std::array<std::vector<uint32_t>, 5>> didYouKnow;
	/// The turns of the last autosave and of the last quick save asked for
	std::optional<uint32_t> lastAutoSaveTurn;
	std::optional<uint32_t> lastQuickSaveTurn;
};

/// The values, from the Locator's services
[[nodiscard]] GameValues CurrentValues();
/// The values a load puts back as it read them, so that a save right after the load writes the record it read: the
/// turn of the last autosave is left out, as the load sets it to the loaded turn rather than putting back the saved one
[[nodiscard]] GameValues ValuesOf(const saved_game_load::LoadedGameState& state);

/// The game object's record with the values put where the game's save writes them, over `base` (the record a load
/// kept, or a new one): the land's number; in the game's data both seeds, the turn and the creation counter; in the
/// words after the lists the influence multipliers, the visual time, its cycle and its speed; in the tail the snow
/// lying (a grid of another size than the record's leaves it), each category's texts read with their count, and the
/// turns of the last autosave and quick save
[[nodiscard]] gamesave::GameHead HeadOf(const GameValues& values, gamesave::GameHead base);

} // namespace openblack::ecs::saved_game_save
