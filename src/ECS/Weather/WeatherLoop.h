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

// The weather's calls from the game loop (Magic/MagicLoop.cpp calls these in the slots of GGame::ProcessTurn 0x54E5C0).

namespace openblack::weather
{
/// A new land, before its script: no climate, weather thing or storm; the grid and the counters back to the start
void OnLoadMap();
/// Slot 1: LH3DAtmos::UpdateGame(VisualTime, 0.1) 0x8356E0, the first thing of a game turn
void ProcessTurnStart(uint32_t turn);
/// Slot 12: WeatherThing::ProcessWeatherThings 0x7741A0 and GClimate::ProcessAll 0x771BE0, after the scripts (the turn
/// of the last ProcessTurnStart); then the test hooks
void ProcessTurnEnd();
/// Every frame (magic::Update, game seconds): LH3DAtmos::Update3D 0x8357A0, the rain streaks (Rain.cpp)
void UpdateFrame(float seconds);
/// OPENBLACK_TEST_WEATHER / OPENBLACK_WEATHER_TRACE (WeatherDebugHooks.cpp), every turn
void RunDebugHooks(uint32_t turn);
void ResetDebugHooks();
} // namespace openblack::weather
