/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SavedGameSave.h"

#include <bit>
#include <span>
#include <utility>

#include "3D/DayNightClock.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"

using namespace openblack::ecs;
using namespace openblack::ecs::saved_game_load;

namespace
{
constexpr size_t k_DidYouKnowCategories = 5;

std::vector<uint32_t> WordsOf(std::span<const float> values)
{
	std::vector<uint32_t> words;
	words.reserve(values.size());
	for (const auto value : values)
	{
		words.push_back(std::bit_cast<uint32_t>(value));
	}
	return words;
}
} // namespace

saved_game_save::GameValues saved_game_save::CurrentValues()
{
	GameValues values;
	if (Locator::gameRandom::has_value())
	{
		values.seeds = game_random::Current();
	}
	if (Locator::time::has_value())
	{
		values.gameTurn = game_clock::Turn();
	}
	if (Locator::objectCreationIndexSystem::has_value())
	{
		values.createdObjects = object_index::Peek();
	}
	if (Locator::mapScriptSystem::has_value())
	{
		const auto& globals = Locator::mapScriptSystem::value().Globals();
		values.landNumber = static_cast<uint32_t>(globals.landNumber);
		values.influenceMultipliers = std::array {globals.playerInfluenceMultiplier, globals.townInfluenceMultiplier};
	}
	if (Locator::dayNightClock::has_value())
	{
		const auto& clock = Locator::dayNightClock::value().Clock();
		values.clock = GameValues::Clock {.visualTime = clock.GetVisualTime(),
		                                  .cycle = {clock.GetDuration(), clock.GetChange(), clock.GetNight()},
		                                  .scale = clock.GetScale()};
	}
	if (Locator::snowSystem::has_value())
	{
		values.snow = Locator::snowSystem::value().Saved();
	}
	if (Locator::scriptState::has_value())
	{
		std::array<std::vector<uint32_t>, k_DidYouKnowCategories> texts;
		for (size_t category = 0; category < texts.size(); ++category)
		{
			const auto read = script_highlight::did_you_know_read::In(static_cast<DykCategory>(category));
			texts.at(category).assign(read.begin(), read.end());
		}
		values.didYouKnow = std::move(texts);
	}
	if (Locator::saveGame::has_value())
	{
		values.lastAutoSaveTurn = Locator::saveGame::value().LastAutoSaveTurn();
		values.lastQuickSaveTurn = Locator::saveGame::value().LastQuickSaveTurn();
	}
	return values;
}

saved_game_save::GameValues saved_game_save::ValuesOf(const LoadedGameState& state)
{
	return {
	    .landNumber = state.landNumber,
	    .seeds = state.seeds,
	    .gameTurn = state.gameTurn,
	    .createdObjects = state.createdObjects,
	    .influenceMultipliers = std::array {state.playerInfluenceMultiplier, state.townInfluenceMultiplier},
	    .clock =
	        GameValues::Clock {.visualTime = state.visualTime, .cycle = state.visualTimeCycle, .scale = state.visualTimeScale},
	    .snow = systems::SavedSnow {.depths = state.snowDepths,
	                                .blockChanges = state.snowBlockChanges,
	                                .meltClock = state.snowMeltClock,
	                                .meltBand = state.snowMeltBand},
	    .didYouKnow = state.didYouKnowTexts,
	    .lastAutoSaveTurn = std::nullopt,
	    .lastQuickSaveTurn = state.lastQuickSaveTurn,
	};
}

openblack::gamesave::GameHead saved_game_save::HeadOf(const GameValues& values, gamesave::GameHead base)
{
	if (values.landNumber.has_value())
	{
		base.landNumber = *values.landNumber;
	}
	if (values.seeds.has_value())
	{
		base.data[k_SyncedSeed] = values.seeds->synced;
		base.data[k_LocalSeed] = values.seeds->local;
	}
	if (values.gameTurn.has_value())
	{
		base.data[k_GameTurn] = *values.gameTurn;
	}
	if (values.createdObjects.has_value())
	{
		base.data[k_CreatedObjects] = *values.createdObjects;
	}
	if (values.influenceMultipliers.has_value())
	{
		base.words[k_PlayerInfluence] = std::bit_cast<uint32_t>((*values.influenceMultipliers)[0]);
		base.words[k_TownInfluence] = std::bit_cast<uint32_t>((*values.influenceMultipliers)[1]);
	}
	if (values.clock.has_value())
	{
		base.words[k_VisualTime] = std::bit_cast<uint32_t>(values.clock->visualTime);
		for (size_t value = 0; value < values.clock->cycle.size(); ++value)
		{
			base.words.at(k_FirstCycleValue + value) = std::bit_cast<uint32_t>(values.clock->cycle.at(value));
		}
		base.words[k_VisualTimeScale] = std::bit_cast<uint32_t>(values.clock->scale);
	}
	// the snow lying: a grid of the record's size (a new record: any), its blocks' changes, the melting clock and band
	if (values.snow.has_value())
	{
		const auto& snow = *values.snow;
		auto& tail = base.tail;
		if (tail.snow.values.empty() || tail.snow.values.size() == snow.depths.size())
		{
			tail.snow.values = WordsOf(snow.depths);
		}
		if (tail.snowSecond.values.empty() || tail.snowSecond.values.size() == snow.blockChanges.size())
		{
			tail.snowSecond.values = WordsOf(snow.blockChanges);
		}
		tail.snowWords[0] = std::bit_cast<uint32_t>(snow.meltClock);
		tail.snowWords[1] = static_cast<uint32_t>(snow.meltBand);
	}
	// each category's texts read, then the same count again, as the game writes them
	if (values.didYouKnow.has_value())
	{
		for (size_t category = 0; category < k_DidYouKnowCategories; ++category)
		{
			const auto& texts = values.didYouKnow->at(category);
			base.tail.didYouKnow.at(category).values = texts;
			base.tail.didYouKnowWords.at(category) = static_cast<uint32_t>(texts.size());
		}
	}
	if (values.lastAutoSaveTurn.has_value())
	{
		base.tail.words[k_TailLastAutoSaveTurn] = *values.lastAutoSaveTurn;
	}
	if (values.lastQuickSaveTurn.has_value())
	{
		base.tail.words[k_TailLastQuickSaveTurn] = *values.lastQuickSaveTurn;
	}
	return base;
}
