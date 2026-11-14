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
#include "Audio/Game/AudioSystem.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/ScriptHighlight.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/CreatureCaveSystemInterface.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "ECS/Systems/ObjectCreationIndexSystemInterface.h"
#include "ECS/Systems/SaveGameSystemInterface.h"
#include "ECS/Systems/ScriptControlSystemInterface.h"
#include "ECS/Systems/ScriptStateInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/SnowSystemInterface.h"
#include "GameClock.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack::ecs;

std::vector<uint32_t> saved_game_load::InterfaceStatusPlayers(const gamesave::WorldRead& world)
{
	std::vector<uint32_t> players;
	for (const auto number : world.loadOrder)
	{
		if (number == 0 || number > world.objects.size() || !world.objects[number - 1].has_value())
		{
			continue;
		}
		if (const auto* status = std::get_if<gamesave::InterfaceStatusRecord>(&*world.objects[number - 1]); status != nullptr)
		{
			players.push_back(status->playerNumber);
		}
	}
	return players;
}

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
	    .frameCount = game->data[k_FrameCount],
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
	// whether the temple's creature room shows the creature, read just before the snow
	state.creatureInTemple = game->tail.cameraWords[k_TailCreatureInTemple];
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
	// whether the game was a skirmish: any other value than 0 is one
	state.skirmish = game->tail.words[k_TailSkirmish] != 0;
	// the script's camera owner and the help's wide screen, read back as they are
	state.cameraOwner = game->tail.scriptSecondWords[k_TailScriptCameraOwner];
	state.wideScreen = game->tail.helpLastWords[k_TailHelpWideScreen];
	state.wideScreenOwner = game->tail.helpLastWords[k_TailHelpWideScreenOwner];
	state.wideScreenTimer = std::bit_cast<float>(game->tail.helpLastWords[k_TailHelpWideScreenTimer]);
	state.clockMs = state.gameTurn * game_clock::k_MsPerTurn;
	return state;
}

void saved_game_load::PutBackSkirmish(Registry& registry, const LoadedGameState& state)
{
	registry.Context().skirmish = state.skirmish;
}

void saved_game_load::Apply(const LoadedGameState& state)
{
	// the game's data: both seeds, and the objects' creation counter (the loaded objects keep their own numbers)
	game_random::Load(state.seeds);
	if (Locator::objectCreationIndexSystem::has_value())
	{
		object_index::Restore(state.createdObjects);
	}
	// and the game loop's passes since the saved land's start
	if (Locator::time::has_value())
	{
		game_clock::SetFrameCount(state.frameCount);
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
	// whether the temple's creature room shows the creature, as a script last said
	if (Locator::creatureCaveSystem::has_value())
	{
		Locator::creatureCaveSystem::value().SetCreatureShown(state.creatureInTemple != 0);
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
	// a skirmish carries on as one
	if (Locator::entitiesRegistry::has_value())
	{
		PutBackSkirmish(Locator::entitiesRegistry::value(), state);
	}
	// the task that had the camera has it again, the camera left as the load made it, so its own end of the camera
	// control gives it back
	if (Locator::scriptControlSystem::has_value())
	{
		Locator::scriptControlSystem::value().RestoreCameraOwner(state.cameraOwner);
	}
	// the wide screen and its owner as saved, without what a change of the wide screen does (the dialogs put away, the
	// interface switched): the bars at their saved timer, and the sounds that the script's wide screen keeps quiet
	if (auto* helpSystem = help::Get(); helpSystem != nullptr)
	{
		helpSystem->RestoreWideScreen(static_cast<int32_t>(state.wideScreen), state.wideScreenOwner);
		audio::SetScriptWideScreen(helpSystem->IsScriptWideScreen());
	}
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const float time =
		    Locator::infoConstants::has_value() ? Locator::infoConstants::value().helpSystem.wideScreenTime : 2.0f;
		Locator::cinematicDirectorSystem::value().RestoreWideScreen(state.wideScreen != 0, state.wideScreenTimer, time);
	}
	// the load's end: the turn, and the clock stopped, set to the turn's time and started again
	game_clock::SetTurn(state.gameTurn);
	game_clock::ResetLocalTimer();
}
