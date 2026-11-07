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

/// What a land's load does to the scripts' own state. A land change (LOAD_MAP, a script going on to the next land)
/// only clears the map: the scripts, their variables and the state they set (the cinema bars, the close clipping, the
/// script camera's lens, the interface interaction, the switches...) stay as they were. Only a new game restarts the
/// scripts, which puts that state back to its start, makes the camera again and ends a film; a playground's start is a
/// new game of its own.
/// docs/wiki/scripts/land-change-and-saving.md#what-a-land-change-keeps
namespace openblack::land_load
{

/// What a load resets besides the map
struct Resets
{
	/// The scripts restart (Game::ResetScripts)
	bool scripts {false};
	/// A new game: the camera is made again (its lens back to 70 degrees), no film goes on and the day / night clock
	/// starts again at noon
	bool newGame {false};
	/// The map is cleared first: every load but the campaign's first land, whose map script the game's start loads with
	/// no clear. The clear's status pass runs with it (HandSystemInterface::ProcessStatusAtClearMap)
	bool mapClear {false};
	/// A script's change of land (LOAD_MAP): a player inside the temple stays inside, the world paused, while the land
	/// beneath is cleared and loaded (LandLoadTemple.h)
	bool keepsTemple {false};
};

/// The game's first land and a playground's start (a land of the Playgrounds folder) are each a new game: the scripts
/// restart and the camera is made again. Every other load is a land change, which keeps both
[[nodiscard]] constexpr Resets ResetsFor(bool firstLand, bool playground)
{
	const bool newGame = firstLand || playground;
	return {.scripts = newGame, .newGame = newGame, .mapClear = !firstLand || playground};
}

/// The game turn the land's script runs at: the map's clear puts the turns back to 0 before it, so that the buildings it
/// makes are not made in play (no footpath from their storage pit, as on the campaign's first land). The campaign's
/// first land has no clear and keeps the turn the game started at
[[nodiscard]] constexpr uint32_t TurnBeforeLandScript(Resets resets, uint32_t turn)
{
	return resets.mapClear ? 0 : turn;
}

} // namespace openblack::land_load
