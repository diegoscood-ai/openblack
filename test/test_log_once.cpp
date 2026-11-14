/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// LogOnce: the script's "Object no longer valid" is an error the first time an opcode logs it on a land, a debug line
// after that. And the program's warnings logged once per process (Locator::logOnce, log_once::First), from any thread

#define LOCATOR_IMPLEMENTATIONS

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "Common/LogOnce.h"
#include "Common/ProcessLogOnce.h"
#include "ECS/Systems/Implementations/LogOnceService.h"
#include "Locator.h"
#include "support/RestoreService.h"

using openblack::LogOnce;
using openblack::ecs::systems::LogOnceService;

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

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LogOnceService, firstOfEachKeyOnly)
{
	LogOnceService logged;
	EXPECT_TRUE(logged.First("graphics/missing-texture/0x1001"));
	EXPECT_FALSE(logged.First("graphics/missing-texture/0x1001"));
	EXPECT_TRUE(logged.First("graphics/missing-texture/0x1002"));
	EXPECT_TRUE(logged.First("ai/todo-state/12"));
	EXPECT_FALSE(logged.First("ai/todo-state/12"));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(LogOnceService, oneThreadOfManyGetsTheFirst)
{
	LogOnceService logged;
	std::atomic<int> firsts {0};
	std::vector<std::thread> threads;
	threads.reserve(8);
	for (int i = 0; i < 8; ++i)
	{
		threads.emplace_back([&logged, &firsts] {
			for (int n = 0; n < 1000; ++n)
			{
				if (logged.First("graphics/uniform/" + std::to_string(n % 10)))
				{
					++firsts;
				}
			}
		});
	}
	for (auto& thread : threads)
	{
		thread.join();
	}
	EXPECT_EQ(firsts.load(), 10);
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ProcessLogOnce, theServiceInTheLocatorAnswers)
{
	const openblack::test::RestoreService<openblack::Locator::logOnce> restore;
	openblack::Locator::logOnce::emplace<LogOnceService>();
	EXPECT_TRUE(openblack::log_once::First("game/banter"));
	EXPECT_FALSE(openblack::log_once::First("game/banter"));
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(ProcessLogOnce, withoutTheServiceEveryWarningIsLogged)
{
	const openblack::test::RestoreService<openblack::Locator::logOnce> restore;
	openblack::Locator::logOnce::reset();
	EXPECT_TRUE(openblack::log_once::First("game/banter"));
	EXPECT_TRUE(openblack::log_once::First("game/banter"));
}
