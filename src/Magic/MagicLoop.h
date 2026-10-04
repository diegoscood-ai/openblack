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

// The miracles' hooks into the game loop (Game.cpp calls each at its step of GGame::ProcessTurn 0x54E5C0, docs
// dev/documentacion/motor/turn_order.md; the sub-systems add their call here, never in Game.cpp). Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic
{
/// The land is loaded (after psys::manager::Clear and before the script runs): the spells, reactions and the players
/// without an entity are cleared
void OnLoadMap();
/// GGame::ProcessOneGameTurn 0x54D620 -> ProcessGameInputs 0x54C3D0, before ProcessGameCode (StartTurn): the hand's
/// casting (GInterface::Process: ProcessPowerUpSystem with the last frame's time)
void ProcessGameInputs();
/// The start of the game turn (GGame::ProcessTurn 0x54E5D7..0x54E646), before GlobalGameLists::Process (the puzzle
/// games) and the villagers: LH3DAtmos::UpdateGame, the influence rings, the players, the dances
void ProcessTurnStart(uint32_t turn);
/// Slot 5, Forest::ProcessForests 0x539D70 (0x54E656): after GlobalGameLists, before Living::ProcessLiving
void ProcessForests(uint32_t turn);
/// Where the original's Living step ends (after livingActionSystem.Update): slots 6..8
void ProcessTurn(uint32_t turn);
/// PSysGlobal::GameLoopEnd 0x68F5B0 (0x54E688), after PhysicsObject::GameTurnUpdate and before GScript::Process: the
/// EXPLODE_OBJECT queue and the PSys sounds of this turn's atoms
void ProcessPSysGameLoopEnd();
/// CHand::GameTurnUpdate 0x46E4E0 (0x54E6F1), after GBelief::ProcessOncePerTurn: the grain's raise and the held
/// object's ProcessInHand
void ProcessHandTurn();
/// Every frame, after the hand's update: seconds of game time (0 while paused)
void Update(float seconds);
/// The environment-variable test hooks (MagicDebugHooks.cpp), every turn; each runs once the land exists
void RunDebugHooks();
/// (OnLoadMap) the hooks run again on the next land
void ResetDebugHooks();
} // namespace openblack::magic
