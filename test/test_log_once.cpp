/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// LogOnce: the script's "Object no longer valid" is an error the first time an opcode logs it on a land, a debug line
// after that

#include <string>

#include <gtest/gtest.h>

#include "Common/LogOnce.h"

using openblack::LogOnce;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LogOnce, firstOfEachKey)
{
	LogOnce logged;
	EXPECT_TRUE(logged.First("GAME_THING_CLICKED"));
	EXPECT_FALSE(logged.First("GAME_THING_CLICKED"));
	EXPECT_TRUE(logged.First("SET_TIMER_TIME"));
	EXPECT_FALSE(logged.First("SET_TIMER_TIME"));
	EXPECT_FALSE(logged.First("GAME_THING_CLICKED"));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LogOnce, keyIsCopied)
{
	LogOnce logged;
	{
		std::string key = "SET_PROPERTY";
		EXPECT_TRUE(logged.First(key));
		key = "SET_FOCUS";
	}
	EXPECT_FALSE(logged.First("SET_PROPERTY"));
	EXPECT_TRUE(logged.First("SET_FOCUS"));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LogOnce, newOneForgetsEverything)
{
	// RegistryContext makes a new one with every land
	LogOnce logged;
	EXPECT_TRUE(logged.First("GET_TIMER_TIME_REMAINING"));
	logged = LogOnce {};
	EXPECT_TRUE(logged.First("GET_TIMER_TIME_REMAINING"));
}
