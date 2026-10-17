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
#include <filesystem>
#include <string>

// The functions that make and take away the services the Locator holds, for the few files that start or end a game

namespace openblack
{
enum class GraphicsBackend : uint8_t;
struct LandData;

namespace windowing
{
enum class DisplayMode : std::uint8_t;
}

void InitializeWindow(const std::string& title, int width, int height, windowing::DisplayMode displayMode, uint32_t extraFlags);
/// The game clock, first of all: the engine timer, the audio device and the game read it
void InitializeClock();
/// The state the game keeps for its whole life, made fresh when the Game is made (the script fade, the day / night
/// clock, the map script's globals and the frame count with its screenshot request)
void InitializeGameState();

/// Which start the services are made for
enum class StartKind : uint8_t
{
	/// A Game's own start: every service is made, as it always was
	Launch,
	/// A new game in a process whose first game has ended: what a process makes once is kept
	NewGame,
};
/// fontHashPermutation: the font cache's glyph hash permutation, drawn right after the atmosphere's start-up, for
/// InitializeGame to keep with the resources.
/// Each call makes the engine's services of a game. What a process makes once (the profiler, the renderer, the file
/// system, the random number manager and the scripts' native functions) is made at a launch, and kept by a new game
bool InitializeEngine(StartKind kind, GraphicsBackend backend, bool vsync,
                      std::array<uint8_t, 256>& fontHashPermutation) noexcept;
/// The game's services; the resources (the asset caches) are made at a launch and kept by a new game, as in
/// InitializeEngine
bool InitializeGame(StartKind kind, const std::array<uint8_t, 256>& fontHashPermutation) noexcept;
void InitializeLevel(const std::filesystem::path& path);
/// The same services for a land made in memory, as the testbed's flat land is
void InitializeLevel(const LandData& land);

/// What a shutdown takes away
enum class ShutDownScope : uint8_t
{
	/// Everything: the program is ending
	Process,
	/// Only what a game makes, before a new game starts in the same process: the window, the renderer, the file system,
	/// the resources (but for the audio's samples, which the audio's start loads with each game), the configuration,
	/// info.dat's values, the profiler, the clock, the random number manager, the scripts' native functions and the mod
	/// loader's result stay
	Game,
};
/// In the reverse order of their dependencies; a game's shutdown also takes the player system and the map, which a
/// process's shutdown leaves to the end of the program
void ShutDownServices(ShutDownScope scope = ShutDownScope::Process);

} // namespace openblack
