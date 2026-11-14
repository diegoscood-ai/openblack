/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// When the game saves itself in play: the autosave's rules and its circle of slots, the quick save's and the quick
// load's, the save on exit and the scripts' slot; the turns of the last autosave and quick save kept in the game
// object's record (docs/wiki/engine/saved-games.md#the-saves-in-play), and a save in play that fails: nothing new
// written, the old files kept, one line of the log and, when the player is to be told, the game's message box

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
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Events/SaveEvents.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/RegistryContext.h"
#include "ECS/SaveTriggers.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedSlot.h"
#include "ECS/Systems/Implementations/SaveGameSystem.h"
#include "ECS/Systems/MapScriptSystemInterface.h"
#include "GameClock.h"
#include "Input/GamePackets.h"
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
// The quick load key asks for a load only more than 3 turns from the turn the last saved game loaded was at, either
// way, so not in a game's first 3 turns before any load; never in a network game or once the game is over; a script's
// wide screen, which holds the quick save back, does not hold it back
TEST(SaveTriggers, TheQuickLoadWaitsMoreThanThreeTurnsFromTheLastLoad)
{
	EXPECT_EQ(QuickLoadRefused(false, false, 100, 103), QuickLoadRefusal::TooSoon);
	EXPECT_EQ(QuickLoadRefused(false, false, 100, 104), std::nullopt);
	EXPECT_EQ(QuickLoadRefused(false, false, 100, 97), QuickLoadRefusal::TooSoon);
	EXPECT_EQ(QuickLoadRefused(false, false, 100, 96), std::nullopt);
	EXPECT_EQ(QuickLoadRefused(false, false, 100, 100), QuickLoadRefusal::TooSoon);
	// a game that has loaded nothing counts from turn 0
	EXPECT_EQ(QuickLoadRefused(false, false, 0, 3), QuickLoadRefusal::TooSoon);
	EXPECT_EQ(QuickLoadRefused(false, false, 0, 4), std::nullopt);
	EXPECT_EQ(QuickLoadRefused(true, false, 0, 50), QuickLoadRefusal::NetworkGame);
	EXPECT_EQ(QuickLoadRefused(false, true, 0, 50), QuickLoadRefusal::GameOver);
	EXPECT_EQ(QuickLoadRefused(true, true, 0, 2), QuickLoadRefusal::NetworkGame);
	EXPECT_EQ(QuickLoadRefused(false, true, 0, 2), QuickLoadRefusal::GameOver);
	// the gap is the same 3 turns as the quick save's
	EXPECT_EQ(k_QuickSaveGap, 3);
	EXPECT_FALSE(QuickLoadRefusalText(QuickLoadRefusal::TooSoon).empty());
	EXPECT_FALSE(QuickLoadRefusalText(QuickLoadRefusal::GameOver).empty());
	EXPECT_FALSE(QuickLoadRefusalText(QuickLoadRefusal::NetworkGame).empty());
	EXPECT_FALSE(QuickLoadRefusalText(QuickLoadRefusal::AlreadyAsked).empty());
}

TEST(SaveTriggers, LeavingTheGameSavesItUnlessItIsOverOrNotAStoryGame)
{
	EXPECT_TRUE(ExitSaves(false, false, false, 1));
	EXPECT_TRUE(ExitSaves(false, false, false, 5));
	EXPECT_FALSE(ExitSaves(true, false, false, 1));
	EXPECT_FALSE(ExitSaves(false, true, false, 1));
	EXPECT_FALSE(ExitSaves(false, false, true, 1));
	EXPECT_FALSE(ExitSaves(false, false, false, k_PlaygroundLand));
}

// Leaving the game saves into the profile played as the Players page was last opened in this game
TEST(SaveTriggers, LeavingTheGameSavesIntoTheProfilePlayedAsThePlayersPageWasOpened)
{
	using enum ExitSaveProfile;
	// the page not opened: the profile played, the game starting again or not
	EXPECT_EQ(ExitSaveProfileOf(u"", u"Bea", false, false), Played);
	EXPECT_EQ(ExitSaveProfileOf(u"", u"Bea", true, false), Played);
	// opened with the profile played, the game not starting again for a pick: that one, whatever the case of its name
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Ann", false, true), Played);
	EXPECT_EQ(ExitSaveProfileOf(u"ann", u"Ann", false, true), Played);
	// another picked since: the one the page was opened with, starting again for the pick or not
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Bea", true, true), Previous);
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Bea", false, true), Previous);
	// that one deleted since: nothing
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Bea", true, false), None);
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Bea", false, false), None);
	// starting again for the profile the page was opened with: nothing
	EXPECT_EQ(ExitSaveProfileOf(u"Ann", u"Ann", true, true), None);
	EXPECT_EQ(ExitSaveProfileOf(u"ANN", u"Ann", true, true), None);
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

// A save keeps whether the game is a skirmish in the game object's tail, as the game writes it, and a load reads it back
// and makes the game a skirmish again, so a loaded skirmish carries on as one; a story game's save keeps 0 there
TEST(SaveTriggers, ASaveRoundTripKeepsTheSkirmish)
{
	const auto skirmishHead = ecs::saved_game_save::HeadOf({.skirmish = true}, gamesave::GameHead {});
	EXPECT_EQ(skirmishHead.tail.words[ecs::saved_game_load::k_TailSkirmish], 1u);
	const auto storyHead = ecs::saved_game_save::HeadOf({.skirmish = false}, skirmishHead);
	EXPECT_EQ(storyHead.tail.words[ecs::saved_game_load::k_TailSkirmish], 0u);

	gamesave::WorldRead world;
	world.objects.emplace_back(skirmishHead);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_TRUE(state->skirmish);
	// the values a load puts back write the same word again
	EXPECT_TRUE(ecs::saved_game_save::ValuesOf(*state).skirmish == std::optional<bool> {true});

	// the load makes the game a skirmish, and a story game's load makes it none
	ecs::Registry registry;
	ecs::saved_game_load::PutBackSkirmish(registry, *state);
	EXPECT_TRUE(registry.Context().skirmish);
	auto story = *state;
	story.skirmish = false;
	ecs::saved_game_load::PutBackSkirmish(registry, story);
	EXPECT_FALSE(registry.Context().skirmish);
}

namespace
{
/// The services a save in play reaches: the save service on files in memory with a profile kept, a world that cannot
/// be written whole (a puzzle game, whose record has no writer yet), the events, and the game's log into a string
class SaveTriggersInPlay: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::saveGame::emplace<ecs::systems::SaveGameSystem>(
		    &files, []() -> std::optional<std::filesystem::path> { return std::filesystem::path("Profiles") / "Player"; });
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		const auto puzzle = registry.Create();
		registry.Assign<ecs::components::PuzzleGame>(puzzle);
		registry.Assign<ecs::components::ObjectCreationIndex>(puzzle, ecs::components::ObjectCreationIndex {.value = 1});
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
	/// The lines logged, but for the readout of every refusal that a refused save the player asked for adds
	[[nodiscard]] size_t LoggedLines() const
	{
		std::istringstream text(logged.str());
		size_t lines = 0;
		for (std::string line; std::getline(text, line);)
		{
			lines += line.find("Save refusal: ") == std::string::npos ? 1 : 0;
		}
		return lines;
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
	// and every reason it could not be made follows it, one line each
	EXPECT_NE(logged.str().find("Save refusal: "), std::string::npos) << logged.str();
	EXPECT_EQ(problemsTold, 1);
	EXPECT_EQ(SaveAsked(3, u"Mine").outcome, ecs::saved_slot::SlotSaveOutcome::NotMade);
	EXPECT_EQ(problemsTold, 2);
}

// The quick load key sends the quick load's packet, which a turn applies by asking the game to load (the load itself
// waits for the frame's end); too soon after the last load it sends nothing. Only the key's packet, never the load,
// happens in the turn
TEST_F(SaveTriggersInPlay, TheQuickLoadKeySendsAPacketThatTheTurnTurnsIntoTheLoad)
{
	game_packets::Reset();
	game_packets::ClearHandlers();
	RegisterPacketHandlers();
	int asked = 0;
	Locator::events::value().AddHandler(std::function([&asked](const ecs::events::QuickLoadAsked& /*asked*/) { ++asked; }));
	auto& saves = Locator::saveGame::value();
	const auto turn = game_clock::Turn();

	// the last load at this very turn: refused, nothing sent
	saves.SetLastLoadTurn(turn);
	EXPECT_EQ(QuickLoadKey(), QuickLoadRefusal::TooSoon);
	EXPECT_EQ(game_packets::Pending(), 0u);

	// far enough from it: the packet waits for the turn, which asks for the load once
	saves.SetLastLoadTurn(turn + 1000);
	EXPECT_EQ(QuickLoadKey(), std::nullopt);
	EXPECT_EQ(game_packets::Pending(), 1u);
	EXPECT_EQ(asked, 0);
	game_packets::Flush();
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(asked, 1);
	EXPECT_EQ(game_packets::Pending(), 0u);
	// nothing was saved or written on the way
	EXPECT_EQ(files.writes, 0);
	EXPECT_EQ(saves.LastLoadTurn(), turn + 1000);

	game_packets::Reset();
	game_packets::ClearHandlers();
}

// Two presses of the quick load key before the turn applies the first ask for one load: the game holds one packet for
// the player, which the second press only writes again; once the turn has applied it, the key may ask again
TEST_F(SaveTriggersInPlay, TwoQuickLoadPressesBeforeTheTurnGiveOneLoad)
{
	game_packets::Reset();
	game_packets::ClearHandlers();
	RegisterPacketHandlers();
	int asked = 0;
	Locator::events::value().AddHandler(std::function([&asked](const ecs::events::QuickLoadAsked& /*asked*/) { ++asked; }));
	Locator::saveGame::value().SetLastLoadTurn(game_clock::Turn() + 1000);

	EXPECT_EQ(QuickLoadKey(), std::nullopt);
	EXPECT_EQ(QuickLoadKey(), QuickLoadRefusal::AlreadyAsked);
	// flushed to the session's list, it still waits for the turn
	game_packets::Flush();
	EXPECT_EQ(QuickLoadKey(), QuickLoadRefusal::AlreadyAsked);
	EXPECT_EQ(game_packets::Pending(), 1u);
	game_packets::DispatchQueuedPackets();
	EXPECT_EQ(asked, 1);
	// applied: a new press sends again
	EXPECT_EQ(QuickLoadKey(), std::nullopt);
	EXPECT_EQ(game_packets::Pending(), 1u);

	game_packets::Reset();
	game_packets::ClearHandlers();
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
