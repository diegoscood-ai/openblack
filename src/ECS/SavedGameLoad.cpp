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
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack::ecs;

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
	    .createdObjects = game->data[k_CreatedObjects],
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
	// the snow lying: its grid, its blocks' changes, then the melting clock and band, as the game writes them
	const auto floats = [](const std::vector<uint32_t>& words) {
		std::vector<float> out;
		out.reserve(words.size());
		for (const auto word : words)
		{
			out.push_back(std::bit_cast<float>(word));
		}
		return out;
	};
	state.snowDepths = floats(game->tail.snow.values);
	state.snowBlockChanges = floats(game->tail.snowSecond.values);
	state.snowMeltClock = std::bit_cast<float>(game->tail.snowWords[0]);
	state.snowMeltBand = static_cast<int32_t>(game->tail.snowWords[1]);
	// the did-you-know texts read: each category's texts, then the count its list holds
	for (size_t category = 0; category < state.didYouKnowTexts.size(); ++category)
	{
		state.didYouKnowTexts.at(category) = game->tail.didYouKnow.at(category).values;
		state.didYouKnowCounts.at(category) = game->tail.didYouKnowWords.at(category);
	}
	// the load reads the last autosave's turn and sets it to the loaded turn, and the clock starts again at the turn's
	// time; the last quick save's turn is the saved one
	state.lastAutoSaveTurn = state.gameTurn;
	state.lastQuickSaveTurn = game->tail.words[k_TailLastQuickSaveTurn];
	state.clockMs = state.gameTurn * game_clock::k_MsPerTurn;
	return state;
}

void saved_game_load::Apply(const LoadedGameState& state)
{
	// the game's data: both seeds, and the objects' creation counter (the loaded objects keep their own numbers)
	game_random::Load(state.seeds);
	if (Locator::objectCreationIndexSystem::has_value())
	{
		object_index::Restore(state.createdObjects);
	}
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
	// the snow lying
	if (Locator::snowSystem::has_value())
	{
		Locator::snowSystem::value().Restore(state.snowDepths, state.snowBlockChanges, state.snowMeltClock, state.snowMeltBand);
	}
	// the did-you-know texts read, category by category
	if (Locator::scriptState::has_value())
	{
		for (size_t category = 0; category < state.didYouKnowTexts.size(); ++category)
		{
			script_highlight::did_you_know_read::Load(static_cast<DykCategory>(category), state.didYouKnowTexts.at(category),
			                                          state.didYouKnowCounts.at(category));
		}
	}
	// the turns of the last autosave and quick save, which the saves keep
	if (Locator::saveGame::has_value())
	{
		Locator::saveGame::value().SetLastAutoSaveTurn(state.lastAutoSaveTurn);
		Locator::saveGame::value().SetLastQuickSaveTurn(state.lastQuickSaveTurn);
	}
	// the load's end: the turn, and the clock stopped, set to the turn's time and started again
	game_clock::SetTurn(state.gameTurn);
	game_clock::ResetLocalTimer();
}
