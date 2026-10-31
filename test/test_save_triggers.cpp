/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// When the game saves itself in play: the autosave's rules and its circle of slots, the quick save's, the save on exit
// and the scripts' slot; the turns of the last autosave and quick save kept in the game object's record
// (docs/wiki/engine/saved-games.md#the-saves-in-play), and a save in play that fails: nothing new written, the old
// files kept, one line of the log and, when the player is to be told, the game's message box

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>

#include <SavePictures.h>
#include <SavedRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/ostream_sink.h>
#include <spdlog/spdlog.h>

#include "Common/EventManager.h"
#include "ECS/Components/MapScriptGlobals.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/Registry.h"
#include "ECS/SaveTriggers.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedSlot.h"
#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/FileSystemFakes.h"

using namespace openblack;
using namespace openblack::ecs::save_triggers;

namespace
{
AutoSaveConditions OnLandOne(uint32_t turn, uint32_t lastAutoSaveTurn)
{
	return {.scriptWideScreen = false,
	        .networkGame = false,
	        .land = 1,
	        .turn = turn,
	        .lastAutoSaveTurn = lastAutoSaveTurn,
	        .paused = false};
}
} // namespace

// The autosave saves once more than 6000 turns went by, not while paused, never in a script's wide screen, a network
// game or the Gods' Playground; forced, it saves whatever the turns and the pause
TEST(SaveTriggers, TheAutosaveSavesEverySixThousandTurnsUnlessHeldBack)
{
	EXPECT_FALSE(AutoSaveDue(OnLandOne(6000, 0), false));
	EXPECT_TRUE(AutoSaveDue(OnLandOne(6001, 0), false));
	EXPECT_FALSE(AutoSaveDue(OnLandOne(7000, 1000), false));
	EXPECT_TRUE(AutoSaveDue(OnLandOne(7001, 1000), false));
	// a turn below the last autosave's (a new land's turn 0) counts as a great many turns
	EXPECT_TRUE(AutoSaveDue(OnLandOne(0, 12000), false));

	auto paused = OnLandOne(6001, 0);
	paused.paused = true;
	EXPECT_FALSE(AutoSaveDue(paused, false));
	EXPECT_TRUE(AutoSaveDue(paused, true));
	EXPECT_TRUE(AutoSaveDue(OnLandOne(5, 0), true));

	auto wideScreen = OnLandOne(6001, 0);
	wideScreen.scriptWideScreen = true;
	auto network = OnLandOne(6001, 0);
	network.networkGame = true;
	auto playground = OnLandOne(6001, 0);
	playground.land = k_PlaygroundLand;
	for (const auto& held : {wideScreen, network, playground})
	{
		EXPECT_FALSE(AutoSaveDue(held, false));
		EXPECT_FALSE(AutoSaveDue(held, true));
	}
}

// Each land's autosaves take turns among slots 15, 16 and 17; the end of a turn asks for it only with the options'
// autosave on and the game not over
TEST(SaveTriggers, TheAutosavesOfALandTakeTurnsAmongThreeSlots)
{
	EXPECT_EQ(CircleSlot(0), 15u);
	EXPECT_EQ(CircleSlot(1), 16u);
	EXPECT_EQ(CircleSlot(2), 17u);
	EXPECT_EQ(CircleSlot(3), 15u);
	EXPECT_EQ(CircleSlot(3001), 16u);
	EXPECT_TRUE(EndTurnAsks(true, false));
	EXPECT_FALSE(EndTurnAsks(false, false));
	EXPECT_FALSE(EndTurnAsks(true, true));
}

// The quick save is asked for only more than 3 turns from the last, either way, and not in a network game, a script's
// wide screen or once the game is over
TEST(SaveTriggers, TheQuickSaveWaitsMoreThanThreeTurnsFromTheLast)
{
	EXPECT_FALSE(QuickSaveAllowed(false, false, false, 100, 103));
	EXPECT_TRUE(QuickSaveAllowed(false, false, false, 100, 104));
	EXPECT_FALSE(QuickSaveAllowed(false, false, false, 100, 97));
	EXPECT_TRUE(QuickSaveAllowed(false, false, false, 100, 96));
	// the first, from turn 0 of the game
	EXPECT_FALSE(QuickSaveAllowed(false, false, false, 0, 2));
	EXPECT_TRUE(QuickSaveAllowed(false, false, false, 0, 4));
	EXPECT_FALSE(QuickSaveAllowed(true, false, false, 0, 50));
	EXPECT_FALSE(QuickSaveAllowed(false, true, false, 0, 50));
	EXPECT_FALSE(QuickSaveAllowed(false, false, true, 0, 50));
}

// Leaving the game saves it, but not once it is over, in a network game, a skirmish or on the Gods' Playground
TEST(SaveTriggers, LeavingTheGameSavesItUnlessItIsOverOrNotAStoryGame)
{
	EXPECT_TRUE(ExitSaves(false, false, false, 1));
	EXPECT_TRUE(ExitSaves(false, false, false, 5));
	EXPECT_FALSE(ExitSaves(true, false, false, 1));
	EXPECT_FALSE(ExitSaves(false, true, false, 1));
	EXPECT_FALSE(ExitSaves(false, false, true, 1));
	EXPECT_FALSE(ExitSaves(false, false, false, k_PlaygroundLand));
}

// A script's slot is its number cut toward zero; below 0, past a whole number of 32 bits or not a number, none
TEST(SaveTriggers, AScriptsSlotIsItsNumberCutTowardZero)
{
	EXPECT_TRUE(ScriptSlot(3.0f) == std::optional<uint32_t> {3});
	EXPECT_TRUE(ScriptSlot(3.9f) == std::optional<uint32_t> {3});
	EXPECT_TRUE(ScriptSlot(-0.5f) == std::optional<uint32_t> {0});
	EXPECT_FALSE(ScriptSlot(-1.0f).has_value());
	EXPECT_FALSE(ScriptSlot(2147483648.0f).has_value());
	EXPECT_FALSE(ScriptSlot(std::numeric_limits<float>::quiet_NaN()).has_value());
}

// The turns of the last autosave and quick save go into the game object's tail; a load reads the quick save's back and
// sets the autosave's to the loaded turn, and the values a load puts back leave the saved autosave's turn as it is
TEST(SaveTriggers, TheLastAutosaveAndQuickSaveTurnsAreInTheGameObjectsTail)
{
	gamesave::GameHead base;
	base.data[ecs::saved_game_load::k_GameTurn] = 900;
	base.tail.words[ecs::saved_game_load::k_TailLastAutoSaveTurn] = 11;
	const auto head = ecs::saved_game_save::HeadOf({.lastAutoSaveTurn = 600, .lastQuickSaveTurn = 750}, base);
	EXPECT_EQ(head.tail.words[ecs::saved_game_load::k_TailLastAutoSaveTurn], 600u);
	EXPECT_EQ(head.tail.words[ecs::saved_game_load::k_TailLastQuickSaveTurn], 750u);

	gamesave::WorldRead world;
	world.objects.emplace_back(head);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->lastAutoSaveTurn, 900u);
	EXPECT_EQ(state->lastQuickSaveTurn, 750u);
	const auto values = ecs::saved_game_save::ValuesOf(*state);
	EXPECT_FALSE(values.lastAutoSaveTurn.has_value());
	EXPECT_TRUE(values.lastQuickSaveTurn == std::optional<uint32_t> {750});
	EXPECT_EQ(ecs::saved_game_save::HeadOf(values, head).tail.words, head.tail.words);
}

namespace
{
/// The services a save in play reaches: the save service on files in memory with a profile kept, an empty world (a game
/// not loaded, whose game object cannot be written whole yet), the events, and the game's log into a string
class SaveTriggersInPlay: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::saveGame::emplace<ecs::systems::SaveGameSystem>(
		    &files, []() -> std::optional<std::filesystem::path> { return std::filesystem::path("Profiles") / "Player"; });
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::events::emplace<EventManager>();
		Locator::events::value().AddHandler(
		    std::function([this](const ecs::events::SaveProblem& /*problem*/) { ++problemsTold; }));
		_previousLog = spdlog::get("game");
		spdlog::drop("game");
		auto log = std::make_shared<spdlog::logger>("game", std::make_shared<spdlog::sinks::ostream_sink_mt>(logged));
		log->set_pattern("%v");
		spdlog::register_logger(log);
	}
	void TearDown() override
	{
		spdlog::drop("game");
		if (_previousLog != nullptr)
		{
			spdlog::register_logger(_previousLog);
		}
		Locator::events::reset();
		Locator::entitiesRegistry::reset();
		Locator::saveGame::reset();
	}

	[[nodiscard]] static uint32_t Land()
	{
		return static_cast<uint32_t>(std::max(0, Locator::mapScriptSystem::value().Globals().landNumber));
	}
	/// The lines the game's log took
	[[nodiscard]] size_t LoggedLines() const
	{
		const auto text = logged.str();
		return static_cast<size_t>(std::ranges::count(text, '\n'));
	}

	test::MemoryFileSystem files;
	std::ostringstream logged;
	int problemsTold {0};

private:
	std::shared_ptr<spdlog::logger> _previousLog;
};
} // namespace

// An autosave that meets a world it cannot write whole writes nothing: the world is made before anything is opened, so
// the slot's old save (its world, script, info and picture cells) stays byte for byte, no temporary file is made, the
// failure is its result and one line of the log, nothing is told on screen and the game goes on (the turn is the last
// autosave's, as after any autosave)
TEST_F(SaveTriggersInPlay, AnAutosaveThatCannotWriteTheWorldKeepsTheOldSaveAndLogsOneLine)
{
	auto& saves = Locator::saveGame::value();
	const auto slot = CircleSlot(0);
	const auto world = *saves.SlotFolder(slot, Land()) / "15.sav";
	files.files[world] = {1, 2, 3, 4};
	files.files[world.parent_path() / "Script_15.sav"] = {5, 6};
	files.files[world.parent_path() / "SaveGame.inf"] = {7};
	const auto pictures = std::filesystem::path("Profiles") / "Player" / "Saved Games" / "Pictures";
	files.files[pictures / gamesave::LargePictureFileName(slot)] = {8};
	files.files[pictures / gamesave::SmallPictureFileName(slot)] = {9};
	const auto before = files.files;

	std::optional<ecs::saved_slot::SlotSave> saved;
	ASSERT_NO_THROW(saved = AutoSave(true));
	ASSERT_TRUE(saved.has_value());
	EXPECT_EQ(saved->outcome, ecs::saved_slot::SlotSaveOutcome::NotMade);
	EXPECT_FALSE(saved->Written());
	EXPECT_FALSE(saved->failure.empty());
	EXPECT_TRUE(files.files == before);
	EXPECT_EQ(files.writes, 0);
	EXPECT_TRUE(files.folders.empty());
	EXPECT_EQ(LoggedLines(), 1u) << logged.str();
	EXPECT_NE(logged.str().find("could not be saved into slot 15 (autosave)"), std::string::npos) << logged.str();
	EXPECT_EQ(problemsTold, 0);
	EXPECT_EQ(saves.LastAutoSaveTurn(), game_clock::Turn());
}

// A save the player asks for that cannot be made fails the same way, and is told on screen by the game's message box
TEST_F(SaveTriggersInPlay, ASaveThePlayerAsksForThatCannotBeMadeIsToldOnScreen)
{
	const auto saved = SaveAsked(k_QuickSaveSlot, u"Mine");
	EXPECT_EQ(saved.outcome, ecs::saved_slot::SlotSaveOutcome::NotMade);
	EXPECT_EQ(files.writes, 0);
	EXPECT_EQ(LoggedLines(), 1u) << logged.str();
	EXPECT_EQ(problemsTold, 1);
	EXPECT_EQ(SaveAsked(3, u"Mine").outcome, ecs::saved_slot::SlotSaveOutcome::NotMade);
	EXPECT_EQ(problemsTold, 2);
}

// A tap on the scroll of a challenge that saves as it is tapped saves into slot 14, as the game's instant save, and is
// not told on screen when it cannot be made (the player did not ask for it); with no profile kept nothing happens
TEST_F(SaveTriggersInPlay, AChallengesScrollTappedSavesIntoSlotFourteen)
{
	EXPECT_EQ(k_ChallengeScrollSlot, 14u);
	ChallengeScrollTapped();
	EXPECT_EQ(files.writes, 0);
	EXPECT_EQ(LoggedLines(), 1u) << logged.str();
	EXPECT_NE(logged.str().find("could not be saved into slot 14 (a challenge's scroll)"), std::string::npos) << logged.str();
	EXPECT_EQ(problemsTold, 0);
	// a run that keeps no profile: no slot, nothing logged
	Locator::saveGame::reset();
	Locator::saveGame::emplace<ecs::systems::SaveGameSystem>(
	    &files, []() -> std::optional<std::filesystem::path> { return std::nullopt; });
	ChallengeScrollTapped();
	EXPECT_EQ(LoggedLines(), 1u) << logged.str();
	EXPECT_EQ(files.writes, 0);
}

// The game's message box tells a save whose files could not be written, whoever asked for it, and a save openblack
// could not make only when the player asked for it; a temporary file that did not open, a save written and one with no
// slot are told nowhere
TEST(SaveTriggers, WhichFailedSavesTheMessageBoxTells)
{
	using ecs::saved_slot::SlotSaveOutcome;
	for (const bool asked : {false, true})
	{
		EXPECT_TRUE(ToldOnScreen(SlotSaveOutcome::WriteFailed, asked));
		EXPECT_FALSE(ToldOnScreen(SlotSaveOutcome::NotOpened, asked));
		EXPECT_FALSE(ToldOnScreen(SlotSaveOutcome::Written, asked));
		EXPECT_FALSE(ToldOnScreen(SlotSaveOutcome::NoSlot, asked));
	}
	EXPECT_TRUE(ToldOnScreen(SlotSaveOutcome::NotMade, true));
	EXPECT_FALSE(ToldOnScreen(SlotSaveOutcome::NotMade, false));
}
