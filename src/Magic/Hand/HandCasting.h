/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Casting from the hand, the per-frame and per-turn parts MagicLoop.cpp calls: the gesture sampling and
// ProcessPowerUpSystem, the in-hand effect, PHandFX and the utility effects. Wiki: docs/bw1-notes/magic.md.

namespace openblack::magic::hand_casting
{
/// A land is loaded
void OnLoadMap();
/// At the start of the game turn (GGame::ProcessGameInputs -> GInterface::Process: ProcessPowerUpSystem again, with
/// the last frame's g_game_time_inc)
void ProcessTurn();
/// Every frame after the hand's update (GGame::ProcessFrameInputs -> GInterface::ProcessFrameUpdates, CHand::Draw):
/// seconds of game time (0 while paused)
void Update(float seconds);
} // namespace openblack::magic::hand_casting
