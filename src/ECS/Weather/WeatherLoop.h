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

// The weather's calls from the game loop: ProcessTurnStart from Magic/MagicLoop.cpp, ProcessWeatherThings and
// ProcessClimate from Game.cpp, each in its slot of the game turn.

namespace openblack::ecs::systems
{
class RainSystemInterface;
class SnowfallSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::weather
{
/// Once a process, at the engine's start-up and before any land, if the Weather detail setting is on: the snow's
/// flakes are scattered and the rain's streaks placed, on the CRT stream. With it off nothing is drawn from the stream
/// and the rain is turned off for the whole process (RainSystemInterface::Disable)
void StartAtmos(bool weather, ecs::systems::SnowfallSystemInterface& snowfall, ecs::systems::RainSystemInterface& rain);
/// A new land, before its script: no climate, weather thing or storm; the grid and the counters back to the start
void OnLoadMap();
/// Slot 1: the atmosphere's game update (VisualTime, 0.1), the first thing of a game turn
void ProcessTurnStart(uint32_t turn);
/// The weather things' turn, after the land alignment's time update
void ProcessWeatherThings();
/// The climates' turn, after the bookmarks and the script highlights (the turn of the last ProcessTurnStart); then
/// the test hooks
void ProcessClimate();
/// The seconds the atmosphere's frame update steps by: the frame's whole game ms in the world, and the frame's whole
/// real ms inside the citadel (sequence mode 1) and while the falling spell's film plays (sequence mode 2), whose
/// real ms go on while the film pauses the game. `realTimeOutsideWorld` off (the View menu's switch, for comparison)
/// takes the game ms there too
[[nodiscard]] float AtmosFrameSeconds(int32_t sequenceMode, uint32_t realMs, uint32_t gameMs, bool realTimeOutsideWorld);
/// Whether the registered storms' clouds take this frame: only in the world (sequence mode 0). The game draws and steps
/// them with the land's draw, which runs neither inside the citadel nor while the falling spell's film plays
[[nodiscard]] bool StormCloudsTakeTheFrame(int32_t sequenceMode) noexcept;
/// Every frame (magic::Update): the atmosphere's 3D update, the storms' flashes, the rain streaks (Rain.cpp) and the
//// falling snow, by AtmosFrameSeconds of this frame, and the storms' clouds where StormCloudsTakeTheFrame
void UpdateFrame();
/// OPENBLACK_TEST_WEATHER / OPENBLACK_WEATHER_TRACE (WeatherDebugHooks.cpp), every turn
void RunDebugHooks(uint32_t turn);
void ResetDebugHooks();
} // namespace openblack::weather
