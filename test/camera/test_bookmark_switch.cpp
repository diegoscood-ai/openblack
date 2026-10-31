/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' switch of the camera bookmarks: on when the game starts, PLAY_JC_SPECIAL 14 turns it on and 15 off,
// and the other specials leave it; and the bookmark keys, which act only while it is on, no script holds the cinema
// bars and the interface interaction leaves the key shortcuts on. docs/wiki/scripts/land1-intro.md (the bookmarks
// switch)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include <LHVM.h>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "ECS/Systems/CameraBookmarkSystemInterface.h"
#include "ECS/Systems/Implementations/CameraBookmarkSystem.h"
#include "Input/BookmarkKeys.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::input::BookmarkKeyActs;
using openblack::input::BookmarkKeyGates;

namespace
{
/// The bookmarks' switch as the scripts set it, every call kept
class FakeBookmarks final: public ecs::systems::CameraBookmarkSystemInterface
{
public:
	bool Initialize() override { return true; }
	void Update(uint32_t /*frameGameMs*/, bool /*scriptWideScreen*/) override {}
	[[nodiscard]] const bookmark_pulse::Pulse& GetPulse() const override { return _pulse; }
	[[nodiscard]] const std::array<entt::entity, 8>& GetBookmarks() const override { return _bookmarks; }
	void SetBookmark(uint8_t /*index*/, const map_coords::MapCoords& /*position*/, std::optional<entt::entity> /*target*/,
	                 const glm::vec3& /*cameraOrigin*/) override
	{
	}
	[[nodiscard]] bool IsSet(uint8_t /*index*/) const override { return false; }
	[[nodiscard]] std::optional<ecs::systems::BookmarkFlight> JumpTo(uint8_t /*index*/,
	                                                                 const ecs::systems::BookmarkJumpFrom& /*from*/) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<ecs::systems::BookmarkFlight>
	ToggleFollow(uint8_t /*index*/, const ecs::systems::BookmarkJumpFrom& /*from*/) override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<uint8_t> GetFollowed() const override { return std::nullopt; }
	[[nodiscard]] uint8_t GetCurrent() const override { return 0; }
	[[nodiscard]] std::optional<ecs::systems::BookmarkFlight> FlightTo(uint8_t /*index*/) const override
	{
		return std::nullopt;
	}
	[[nodiscard]] std::optional<ecs::systems::BookmarkFlight>
	ProcessTurn(bool /*playersCamera*/, bool /*cameraInputCounted*/, const ecs::systems::BookmarkJumpFrom& /*from*/) override
	{
		return std::nullopt;
	}
	void ClearBookmark(uint8_t /*index*/) override {}
	void SetEnabled(bool enabled) override { set.push_back(enabled); }
	[[nodiscard]] bool IsEnabled() const override { return set.empty() || set.back(); }

	std::vector<bool> set;

private:
	std::array<entt::entity, 8> _bookmarks {};
	bookmark_pulse::Pulse _pulse {};
};

class BookmarkSwitch: public ::testing::Test
{
protected:
	void SetUp() override
	{
		// the natives log to the scripts' logger: a null sink, as the other script tests
		if (!spdlog::get("scripting"))
		{
			spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
		}
		_vm = &Locator::vm::emplace<lhvm::LHVM>();
		_bookmarks = &static_cast<FakeBookmarks&>(Locator::cameraBookmarkSystem::emplace<FakeBookmarks>());
	}

	/// PLAY_JC_SPECIAL with the special's number
	void PlayJcSpecial(int32_t special)
	{
		const auto& table = _api.GetFunctionsTable();
		const auto native = std::ranges::find_if(
		    table, [](const lhvm::NativeFunction& f) { return std::string_view(f.name) == "PLAY_JC_SPECIAL"; });
		ASSERT_NE(native, table.end());
		ASSERT_EQ(native->stackIn, 1);
		ASSERT_EQ(native->stackOut, 0);
		_vm->Pushi(special);
		native->impl();
	}

	test::RestoreService<Locator::vm> _restoreVm;
	test::RestoreService<Locator::cameraBookmarkSystem> _restoreBookmarks;
	lhvm::LHVM* _vm {nullptr};
	FakeBookmarks* _bookmarks {nullptr};
	chlapi::CHLApi _api;
};

constexpr BookmarkKeyGates k_Open {.scriptWideScreen = false, .keyShortcutsEnabled = true, .bookmarksEnabled = true};
} // namespace

// The game starts with the bookmarks on, before any script or land turns them on
TEST(BookmarkSwitchStart, TheBookmarksAreOnWhenTheGameStarts)
{
	const ecs::systems::CameraBookmarkSystem bookmarks {};
	EXPECT_TRUE(bookmarks.IsEnabled());
}

// Special 15 turns the bookmarks off, and special 14 turns them on again
TEST_F(BookmarkSwitch, SpecialFifteenTurnsTheBookmarksOffAndFourteenOn)
{
	PlayJcSpecial(15);
	EXPECT_EQ(_bookmarks->set, std::vector<bool> {false});
	PlayJcSpecial(14);
	EXPECT_EQ(_bookmarks->set, (std::vector<bool> {false, true}));
	// Each turns them the same way again
	PlayJcSpecial(14);
	PlayJcSpecial(15);
	EXPECT_EQ(_bookmarks->set, (std::vector<bool> {false, true, true, false}));
}

// The specials that do nothing, from 7 to 13 and above 15 (a negative number counts as above 15), leave the switch
TEST_F(BookmarkSwitch, TheSpecialsThatDoNothingLeaveTheBookmarks)
{
	for (const int32_t special : {7, 8, 9, 10, 11, 12, 13, 16, 18, -1})
	{
		PlayJcSpecial(special);
	}
	EXPECT_TRUE(_bookmarks->set.empty());
}

// With nothing in the way a bookmark key acts
TEST(BookmarkKeys, ABookmarkKeyActsWithNothingInTheWay)
{
	EXPECT_TRUE(BookmarkKeyActs(k_Open));
}

// The scripts' bookmarks switch off, the keys do nothing
TEST(BookmarkKeys, NoBookmarkKeyActsWhileTheScriptsHaveTheBookmarksOff)
{
	auto gates = k_Open;
	gates.bookmarksEnabled = false;
	EXPECT_FALSE(BookmarkKeyActs(gates));
}

// A script holding the cinema bars stops the keys
TEST(BookmarkKeys, NoBookmarkKeyActsWhileAScriptHoldsTheCinemaBars)
{
	auto gates = k_Open;
	gates.scriptWideScreen = true;
	EXPECT_FALSE(BookmarkKeyActs(gates));
}

// The interface interaction's key shortcuts off stop the keys
TEST(BookmarkKeys, NoBookmarkKeyActsWhileTheKeyShortcutsAreOff)
{
	auto gates = k_Open;
	gates.keyShortcutsEnabled = false;
	EXPECT_FALSE(BookmarkKeyActs(gates));
}

// The modifiers held with a bookmark key: Ctrl alone drops, Shift and Ctrl clear, Shift follows, the key alone flies,
// and Ctrl with Alt, or Alt alone, does nothing
TEST(BookmarkKeys, TheModifiersChooseWhatTheKeyDoes)
{
	using openblack::input::ActionOfBookmarkKey;
	using openblack::input::BookmarkKeyAction;
	EXPECT_EQ(ActionOfBookmarkKey(false, true, false), BookmarkKeyAction::Drop);
	EXPECT_EQ(ActionOfBookmarkKey(true, true, false), BookmarkKeyAction::Clear);
	EXPECT_EQ(ActionOfBookmarkKey(true, true, true), BookmarkKeyAction::Clear);
	EXPECT_EQ(ActionOfBookmarkKey(true, false, false), BookmarkKeyAction::Follow);
	EXPECT_EQ(ActionOfBookmarkKey(true, false, true), BookmarkKeyAction::Follow);
	EXPECT_EQ(ActionOfBookmarkKey(false, false, false), BookmarkKeyAction::FlyTo);
	EXPECT_EQ(ActionOfBookmarkKey(false, true, true), BookmarkKeyAction::None);
	EXPECT_EQ(ActionOfBookmarkKey(false, false, true), BookmarkKeyAction::None);
}

// The number keys 1 to 7 are bookmark keys, and 8 is not
TEST(BookmarkKeys, TheKeysOneToSevenAreBookmarkKeys)
{
	for (int number = 1; number <= 7; ++number)
	{
		EXPECT_TRUE(openblack::input::IsBookmarkKey(number)) << number;
	}
	EXPECT_FALSE(openblack::input::IsBookmarkKey(8));
	EXPECT_FALSE(openblack::input::IsBookmarkKey(0));
}

// Backspace acts as the bookmark keys do, but with the bookmarks off too
TEST(BookmarkKeys, BackspaceActsWithTheBookmarksOff)
{
	using openblack::input::BeforeJumpKeyActs;
	EXPECT_TRUE(BeforeJumpKeyActs(k_Open));
	auto gates = k_Open;
	gates.bookmarksEnabled = false;
	EXPECT_TRUE(BeforeJumpKeyActs(gates));
	gates = k_Open;
	gates.scriptWideScreen = true;
	EXPECT_FALSE(BeforeJumpKeyActs(gates));
	gates = k_Open;
	gates.keyShortcutsEnabled = false;
	EXPECT_FALSE(BeforeJumpKeyActs(gates));
}
