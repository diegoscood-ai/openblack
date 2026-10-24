/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedGameLoad.h"

#include <bit>
#include <variant>

#include <SavedRecords.h>
#include <WorldReader.h>

#include "3D/DayNightClock.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack::ecs;

namespace
{
/// Where the game's data, the bytes after it and the ten words after the global lists keep each value
constexpr size_t k_SyncedSeed = 0;
constexpr size_t k_LocalSeed = 1;
constexpr size_t k_GameTurn = 2;
constexpr size_t k_RealGameTurn = 3;
constexpr size_t k_PlayerIndex = 0;
constexpr size_t k_RenderTime = 1;
constexpr size_t k_RenderTimeIncrement = 2;
constexpr size_t k_PlayerInfluence = 3;
constexpr size_t k_TownInfluence = 4;
constexpr size_t k_VisualTime = 5;
constexpr size_t k_FirstCycleValue = 6;
constexpr size_t k_VisualTimeScale = 9;
} // namespace

std::optional<saved_game_load::LoadedGameState> saved_game_load::StateOf(const gamesave::WorldRead& world)
{
	if (world.objects.empty() || !world.objects.front().has_value())
	{
		return std::nullopt;
	}
	const auto* game = std::get_if<gamesave::GameHead>(&*world.objects.front());
	if (game == nullptr)
	{
		return std::nullopt;
	}
	LoadedGameState state {
	    .landscape = world.framing.landscape,
	    .landNumber = game->landNumber,
	    .gameFlags = game->gameFlags,
	    .gameTurn = game->data[k_GameTurn],
	    .realGameTurn = game->data[k_RealGameTurn],
	    .seeds = {.synced = game->data[k_SyncedSeed], .local = game->data[k_LocalSeed]},
	    .playerIndex = game->bytes[k_PlayerIndex],
	    .renderTime = game->words[k_RenderTime],
	    .renderTimeIncrement = game->words[k_RenderTimeIncrement],
	    .playerInfluenceMultiplier = std::bit_cast<float>(game->words[k_PlayerInfluence]),
	    .townInfluenceMultiplier = std::bit_cast<float>(game->words[k_TownInfluence]),
	    .visualTime = std::bit_cast<float>(game->words[k_VisualTime]),
	    .visualTimeCycle = {std::bit_cast<float>(game->words[k_FirstCycleValue]),
	                        std::bit_cast<float>(game->words[k_FirstCycleValue + 1]),
	                        std::bit_cast<float>(game->words[k_FirstCycleValue + 2])},
	    .visualTimeScale = std::bit_cast<float>(game->words[k_VisualTimeScale]),
	};
	// the load sets the last autosave's turn to the loaded turn, and the clock starts again at the turn's time
	state.lastAutoSaveTurn = state.gameTurn;
	state.clockMs = state.gameTurn * game_clock::k_MsPerTurn;
	return state;
}

void saved_game_load::Apply(const LoadedGameState& state)
{
	// the game's data: both seeds
	game_random::Load(state.seeds);
	// the land's number and the influence multipliers, which the land's script keeps
	if (Locator::mapScriptSystem::has_value())
	{
		auto& globals = Locator::mapScriptSystem::value().Globals();
		globals.landNumber = static_cast<int32_t>(state.landNumber);
		globals.playerInfluenceMultiplier = state.playerInfluenceMultiplier;
		globals.townInfluenceMultiplier = state.townInfluenceMultiplier;
	}
	// the time of day forced to the saved one, then its speed and its cycle (duration, night, change)
	if (Locator::dayNightClock::has_value())
	{
		auto& clock = Locator::dayNightClock::value().Clock();
		clock.ForceVisualTime(state.visualTime);
		clock.SetScale(state.visualTimeScale);
		clock.SetCycle(state.visualTimeCycle[0], state.visualTimeCycle[2], state.visualTimeCycle[1]);
		if (Locator::skySystem::has_value())
		{
			Locator::skySystem::value().SetTime(clock.GetScriptTime());
		}
	}
	// the load's end: the turn, and the clock stopped, set to the turn's time and started again
	game_clock::SetTurn(state.gameTurn);
	game_clock::ResetLocalTimer();
}
