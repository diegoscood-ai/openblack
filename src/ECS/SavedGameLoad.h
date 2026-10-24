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
#include <string>

#include "Common/GameRandom.h"

namespace openblack::gamesave
{
struct WorldRead;
}

// What a load of a saved game sets from the game object's record, before any object of the world: the land's file,
// the turn, the seeds, the render clock, the influence multipliers and the time of day. A pure reading of the world
// file, so it can be checked on the game's own saves without the game. See
// docs/wiki/engine/saved-games.md#what-a-load-does.
namespace openblack::ecs::saved_game_load
{

/// The game object's values a load puts back first
struct LoadedGameState
{
	/// The land's file as the game opened it
	std::string landscape;
	uint32_t landNumber {0};
	uint32_t gameFlags {0};
	/// The game's turn and the turn of the real clock
	uint32_t gameTurn {0};
	uint32_t realGameTurn {0};
	/// The synchronised seed and the local one
	GameRandomSeeds seeds;
	/// The player of this machine
	uint8_t playerIndex {0};
	uint32_t renderTime {0};
	uint32_t renderTimeIncrement {0};
	float playerInfluenceMultiplier {0.0f};
	float townInfluenceMultiplier {0.0f};
	/// The time of day, its cycle's three values and its speed
	float visualTime {0.0f};
	std::array<float, 3> visualTimeCycle {};
	float visualTimeScale {0.0f};
	/// The turn of the last autosave: the loaded turn
	uint32_t lastAutoSaveTurn {0};
	/// The clock starts again at the turn times the turn's length
	uint32_t clockMs {0};
};

/// The values, or none when the game object's record was not read
[[nodiscard]] std::optional<LoadedGameState> StateOf(const gamesave::WorldRead& world);

/// Puts the values back as the game object's load does, in its order: the seeds, the land's number, the influence
/// multipliers, the time of day forced (with the sky), its speed and its cycle; then, as the load's end does, the turn
/// and the clock started again at the turn's time. Through the Locator's services
void Apply(const LoadedGameState& state);

} // namespace openblack::ecs::saved_game_load
