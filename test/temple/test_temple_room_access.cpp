/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which rooms of the temple are open to the player (Temple/TempleRoomAccess.h), and the help a closed room's door
// starts (Help/ScriptControl.h)

#include <cstdint>

#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "Help/ScriptControl.h"
#include "Temple/TempleKeys.h"
#include "Temple/TempleRoomAccess.h"

using namespace openblack;

TEST(TempleRoomAccess, OnlyTheMainRoomIsOpenUntilItHasBeenDrawn)
{
	using temple::IsRoomAvailable;
	const temple::RoomAccessFacts closed {.roomsOpened = false, .hasCreature = true, .keptIn = std::nullopt};
	EXPECT_TRUE(IsRoomAvailable(TempleRoom::Main, closed));
	for (const auto room : {TempleRoom::CreatureCave, TempleRoom::Challenge, TempleRoom::Multi, TempleRoom::Options,
	                        TempleRoom::SaveGame, TempleRoom::Credits, TempleRoom::Unknown})
	{
		EXPECT_FALSE(IsRoomAvailable(room, closed));
	}

	const temple::RoomAccessFacts open {.roomsOpened = true, .hasCreature = true, .keptIn = std::nullopt};
	for (const auto room : {TempleRoom::Main, TempleRoom::CreatureCave, TempleRoom::Challenge, TempleRoom::Multi,
	                        TempleRoom::Options, TempleRoom::SaveGame, TempleRoom::Credits})
	{
		EXPECT_TRUE(IsRoomAvailable(room, open));
	}
	EXPECT_FALSE(IsRoomAvailable(TempleRoom::Unknown, open));
}

TEST(TempleRoomAccess, TheCreaturesRoomNeedsACreature)
{
	using temple::IsRoomAvailable;
	EXPECT_FALSE(
	    IsRoomAvailable(TempleRoom::CreatureCave, {.roomsOpened = true, .hasCreature = false, .keptIn = std::nullopt}));
	EXPECT_TRUE(IsRoomAvailable(TempleRoom::Challenge, {.roomsOpened = true, .hasCreature = false, .keptIn = std::nullopt}));
}

TEST(TempleRoomAccess, TheRoomThePlayerIsKeptInIsOpen)
{
	using temple::IsRoomAvailable;
	EXPECT_TRUE(
	    IsRoomAvailable(TempleRoom::SaveGame, {.roomsOpened = false, .hasCreature = false, .keptIn = TempleRoom::SaveGame}));
	EXPECT_TRUE(IsRoomAvailable(TempleRoom::Multi, {.roomsOpened = false, .hasCreature = false, .keptIn = TempleRoom::Multi}));
	// Only these two rooms open when the game keeps the player in them
	EXPECT_FALSE(
	    IsRoomAvailable(TempleRoom::Credits, {.roomsOpened = false, .hasCreature = false, .keptIn = TempleRoom::Credits}));
	EXPECT_FALSE(
	    IsRoomAvailable(TempleRoom::Multi, {.roomsOpened = false, .hasCreature = false, .keptIn = TempleRoom::SaveGame}));
}

namespace
{
/// What the script VM was asked: the type masks stopped and the scripts started with their masks
struct VmCalls
{
	std::vector<uint32_t> stopped;
	std::vector<std::pair<std::string, uint32_t>> started;
};

help::script_control::Vm FakeVm(VmCalls& calls, bool multiplayer)
{
	help::script_control::Vm vm;
	vm.stopTasksOfType = [&calls](uint32_t mask) { calls.stopped.push_back(mask); };
	vm.startScript = [&calls](std::string_view name, uint32_t mask) { calls.started.emplace_back(std::string(name), mask); };
	vm.multiplayer = [multiplayer]() { return multiplayer; };
	return vm;
}
} // namespace

TEST(TempleRoomAccess, AClosedRoomsDoorStopsTheHelpAndSaysSo)
{
	VmCalls calls;
	help::script_control::RoomNotAvailable(FakeVm(calls, false));
	ASSERT_EQ(calls.stopped.size(), 1u);
	EXPECT_EQ(calls.stopped[0], help::script_control::k_HelpScriptTypes);
	ASSERT_EQ(calls.started.size(), 1u);
	EXPECT_EQ(calls.started[0].first, "CitadelWorldRoomNotAvailable");
	EXPECT_EQ(calls.started[0].second, help::script_control::k_SinglePlayerScriptTypes);
}

TEST(TempleRoomAccess, AMultiplayerGameStartsNoTempleScript)
{
	VmCalls calls;
	help::script_control::RoomNotAvailable(FakeVm(calls, true));
	EXPECT_EQ(calls.stopped.size(), 1u);
	EXPECT_TRUE(calls.started.empty());
}

TEST(TempleKeys, EscapeInTheMainRoomLeaves)
{
	EXPECT_EQ(temple::OnEscape(TempleRoom::Main, std::nullopt), temple::EscapeResult::Leave);
}

TEST(TempleKeys, EscapeInAnyOtherRoomGoesBackToTheMainRoom)
{
	for (const auto room : {TempleRoom::Challenge, TempleRoom::CreatureCave, TempleRoom::Credits, TempleRoom::Multi,
	                        TempleRoom::Options, TempleRoom::SaveGame})
	{
		EXPECT_EQ(temple::OnEscape(room, std::nullopt), temple::EscapeResult::GoToMainRoom);
	}
}

TEST(TempleKeys, WhileTheGameKeepsThePlayerInARoomEscapeInTheSaveGameRoomLeaves)
{
	EXPECT_EQ(temple::OnEscape(TempleRoom::SaveGame, TempleRoom::SaveGame), temple::EscapeResult::Leave);
	EXPECT_EQ(temple::OnEscape(TempleRoom::Challenge, TempleRoom::SaveGame), temple::EscapeResult::GoToMainRoom);
	EXPECT_EQ(temple::OnEscape(TempleRoom::Main, TempleRoom::SaveGame), temple::EscapeResult::Leave);
}
