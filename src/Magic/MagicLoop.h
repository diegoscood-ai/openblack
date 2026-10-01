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

// The miracles' hooks into the game loop (Game.cpp calls these five; the sub-systems add their call here, never in
// Game.cpp). The order is GGame::ProcessTurn 0x54E5C0's. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
/// The land is loaded (after psys::manager::Clear and before the script runs): the spells, reactions and the players
/// without an entity are cleared
void OnLoadMap();
/// One game turn, where the original's Living step ends (after livingActionSystem.Update): slots 1..8
void ProcessTurn(uint32_t turn);
/// The rest of the turn, after psys::manager::ProcessTurn (the original's slot 9, which openblack runs at the end of
/// the scripts block): slots 11..14, so that the PSys sounds see this turn's atoms
void ProcessTurnEnd();
/// Every frame, after the hand's update: seconds of game time (0 while paused)
void Update(float seconds);
/// The environment-variable test hooks (MagicDebugHooks.cpp), every turn; each runs once the land exists
void RunDebugHooks();
/// (OnLoadMap) the hooks run again on the next land
void ResetDebugHooks();
} // namespace openblack::magic
