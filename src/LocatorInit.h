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

namespace player_profiles
{
struct Store;
}

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
/// The services the simulation needs first of all, with no window, renderer or audio device: the warnings logged once,
/// the profiler, the events, the files and the random numbers. The profiler, the file system and the random number
/// manager are made at a launch, and kept by a new game
void InitializeSimulationCore(StartKind kind);
/// fontHashPermutation: the font cache's glyph hash permutation, drawn right after the atmosphere's start-up, for
/// InitializeSimulation to keep with the resources.
/// The services that show the world, made after the core services: the renderer, the debug GUI and the audio, and the
/// services made among them (the particles, the script state, the world's effects and weather and the physics), then
/// the atmosphere's start-up and the wave device. What a process makes once, the renderer, is made at a launch and kept
/// by a new game
bool InitializeEngine(StartKind kind, GraphicsBackend backend, bool vsync,
                      std::array<uint8_t, 256>& fontHashPermutation) noexcept;
/// The player profiles of the store, read and written through the file system service, made once at a launch, after
/// the game path is known and before InitializeGame; a restart and a new game keep them (a test's fake stays)
void InitializePlayerProfiles(const player_profiles::Store& store);
/// The script machine and the systems of the world; the scripts' native functions and the resources (the asset caches)
/// are made at a launch and kept by a new game, as in InitializeEngine
bool InitializeSimulation(StartKind kind, const std::array<uint8_t, 256>& fontHashPermutation) noexcept;
/// The services of the game on the player's computer, made after the simulation's: the input's action map, the drawing
/// of the world, the editor and the films
void InitializeClient();
/// The game's services: the simulation, then the services of the game on the player's computer
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
