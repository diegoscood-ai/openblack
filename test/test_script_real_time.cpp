/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the scripts read of the computer's clock: GET_REAL_TIME, both GET_REAL_DAY, GET_REAL_MONTH and GET_REAL_YEAR,
// pinned to the original's answers for the same local times (the exe oracle, with the clock's two calls stubbed).
// docs/wiki/world/day-and-night.md#the-computers-clock-in-the-scripts

#include <cstdint>
#include <ctime>

#include <algorithm>
#include <array>
#include <bit>
#include <string_view>

#include <LHVM.h>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "CHLApi.h"
#include "Locator.h"
#include "ScriptHeaders/ScriptRealTime.h"
#include "support/RestoreService.h"

using namespace openblack;
namespace real_time = script::real_time;

namespace
{
struct Case
{
	int second, minute, hour, monthDay, month, yearsSince1900, weekDay;
	uint32_t time, monthDayBits, weekDayBits, monthBits, yearBits;
};

// The original's pushed bits for each local time
constexpr std::array<Case, 5> k_Cases {{
    {0, 0, 0, 1, 0, 0, 0, 0x00000000, 0x3F800000, 0x3F800000, 0x3F800000, 0x44ED8000},
    // 1:01:14 is one of the times where working in double would round the other way
    {14, 1, 1, 31, 11, 126, 6, 0x3F82A190, 0x41F80000, 0x40E00000, 0x41400000, 0x44FD4000},
    {59, 59, 23, 15, 6, 100, 3, 0x41BFFF6E, 0x41700000, 0x40800000, 0x40E00000, 0x44FA0000},
    {30, 30, 12, 29, 1, 124, 4, 0x41482222, 0x41E80000, 0x40A00000, 0x40000000, 0x44FD0000},
    {7, 13, 5, 9, 3, 99, 2, 0x40A6FEDD, 0x41100000, 0x40400000, 0x40800000, 0x44F9E000},
}};

std::tm LocalTime(const Case& c)
{
	std::tm t {};
	t.tm_sec = c.second;
	t.tm_min = c.minute;
	t.tm_hour = c.hour;
	t.tm_mday = c.monthDay;
	t.tm_mon = c.month;
	t.tm_year = c.yearsSince1900;
	t.tm_wday = c.weekDay;
	return t;
}
} // namespace

TEST(ScriptRealTime, AnswersAsTheOriginalForTheSameLocalTime)
{
	for (const auto& c : k_Cases)
	{
		const auto t = LocalTime(c);
		EXPECT_EQ(std::bit_cast<uint32_t>(real_time::Time(t)), c.time) << c.hour << ':' << c.minute << ':' << c.second;
		EXPECT_EQ(std::bit_cast<uint32_t>(real_time::MonthDay(t)), c.monthDayBits);
		EXPECT_EQ(std::bit_cast<uint32_t>(real_time::WeekDay(t)), c.weekDayBits);
		EXPECT_EQ(std::bit_cast<uint32_t>(real_time::Month(t)), c.monthBits);
		EXPECT_EQ(std::bit_cast<uint32_t>(real_time::Year(t)), c.yearBits);
	}
}

TEST(ScriptRealTime, TheNativesTakeNothingAndGiveOneNumber)
{
	if (!spdlog::get("scripting"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("scripting");
	}
	test::RestoreService<Locator::vm> restoreVm;
	Locator::vm::emplace<lhvm::LHVM>();
	chlapi::CHLApi api;
	const auto& table = api.GetFunctionsTable();
	int found = 0;
	for (const auto& native : table)
	{
		const std::string_view name(native.name);
		if (name != "GET_REAL_TIME" && name != "GET_REAL_DAY" && name != "GET_REAL_MONTH" && name != "GET_REAL_YEAR")
		{
			continue;
		}
		++found;
		EXPECT_EQ(native.stackIn, 0) << name;
		EXPECT_EQ(native.stackOut, 1u) << name;
		// a value left below shows that the native took nothing and left one
		Locator::vm::value().Pushf(-5.0f);
		native.impl();
		const float answer = Locator::vm::value().Popf();
		EXPECT_EQ(Locator::vm::value().Popf(), -5.0f) << name;
		EXPECT_GE(answer, 0.0f) << name;
	}
	// GET_REAL_DAY is listed twice, the day of the month and the day of the week
	EXPECT_EQ(found, 5);
}
