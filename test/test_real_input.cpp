/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The filter of the real mouse's events for test runs (Input/RealInput), with fake event types

#include <cstdint>

#include <array>
#include <optional>
#include <string>

#include <SDL_events.h>
#include <gtest/gtest.h>

#include "Input/GameCursor.h"
#include "Input/RealInput.h"

using namespace openblack::input;

namespace
{
constexpr std::array<uint32_t, 4> k_MouseEvents = {SDL_MOUSEMOTION, SDL_MOUSEBUTTONDOWN, SDL_MOUSEBUTTONUP, SDL_MOUSEWHEEL};
constexpr std::array<uint32_t, 5> k_OtherEvents = {SDL_KEYDOWN, SDL_KEYUP, SDL_TEXTINPUT, SDL_QUIT, SDL_WINDOWEVENT};
} // namespace

TEST(RealInput, theMouseEventsAreMotionButtonsAndWheel)
{
	for (const auto type : k_MouseEvents)
	{
		EXPECT_TRUE(IsMouseEvent(type)) << type;
	}
	for (const auto type : k_OtherEvents)
	{
		EXPECT_FALSE(IsMouseEvent(type)) << type;
	}
}

TEST(RealInput, withNeitherSwitchEveryEventGoesThrough)
{
	for (const auto type : k_MouseEvents)
	{
		EXPECT_FALSE(DropRealEvent(type, false, false)) << type;
	}
	for (const auto type : k_OtherEvents)
	{
		EXPECT_FALSE(DropRealEvent(type, false, false)) << type;
	}
}

TEST(RealInput, ignoringTheRealInputDropsOnlyTheMouse)
{
	for (const auto type : k_MouseEvents)
	{
		EXPECT_TRUE(DropRealEvent(type, false, true)) << type;
	}
	for (const auto type : k_OtherEvents)
	{
		EXPECT_FALSE(DropRealEvent(type, false, true)) << type;
	}
}

TEST(RealInput, aFixedMouseDropsTheMouseAsBefore)
{
	for (const auto type : k_MouseEvents)
	{
		EXPECT_TRUE(DropRealEvent(type, true, false)) << type;
		EXPECT_TRUE(DropRealEvent(type, true, true)) << type;
	}
	for (const auto type : k_OtherEvents)
	{
		EXPECT_FALSE(DropRealEvent(type, true, false)) << type;
	}
}

TEST(MouseAt, TheEnvironmentUnlessATestHookReplacedIt)
{
	const char* environment = "0.5,0.6";
	EXPECT_EQ(MouseAtFrom(std::nullopt, environment), environment);
	EXPECT_EQ(MouseAtFrom(std::nullopt, nullptr), nullptr);
	const std::optional<std::string> moved = "0.2,0.3";
	EXPECT_STREQ(MouseAtFrom(moved, environment), "0.2,0.3");
	EXPECT_STREQ(MouseAtFrom(moved, nullptr), "0.2,0.3");
	// an empty replacement unsets it, as setting the variable to nothing does
	EXPECT_EQ(MouseAtFrom(std::string {}, environment), nullptr);
}

// A test run's fixed cursors pin the frame's cursor, OPENBLACK_MOUSE_AT's over OPENBLACK_FIXED_MOUSE's, but not while a
// hand demo plays: then the cursor is the one the playback wrote
TEST(MouseAt, AHandDemoKeepsItsOwnCursorOverTheTestRunsFixedOnes)
{
	using openblack::input::FrameCursor;
	const glm::ivec2 game(448, 233);
	const glm::ivec2 fixed(10, 20);
	const glm::ivec2 mouseAt(512, 384);
	EXPECT_EQ(FrameCursor(game, std::nullopt, std::nullopt, false), game);
	EXPECT_EQ(FrameCursor(game, fixed, std::nullopt, false), fixed);
	EXPECT_EQ(FrameCursor(game, fixed, mouseAt, false), mouseAt);
	EXPECT_EQ(FrameCursor(game, std::nullopt, mouseAt, false), mouseAt);
	EXPECT_EQ(FrameCursor(game, fixed, mouseAt, true), game);
	EXPECT_EQ(FrameCursor(game, std::nullopt, mouseAt, true), game);
}
