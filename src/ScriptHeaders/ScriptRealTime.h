/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <ctime>

/// What the scripts read of the player's computer clock (GET_REAL_TIME, GET_REAL_DAY, GET_REAL_MONTH, GET_REAL_YEAR),
/// from its local time (docs/wiki/world/day-and-night.md#the-computers-clock-in-the-scripts)
namespace openblack::script::real_time
{
/// The computer's local time now
[[nodiscard]] std::tm Now();

/// GET_REAL_TIME: the hour with the minutes and seconds as its fraction, each step rounded to a float as the original
[[nodiscard]] float Time(const std::tm& now);

/// GET_REAL_DAY (115): the day of the month, 1 to 31
[[nodiscard]] float MonthDay(const std::tm& now);

/// GET_REAL_DAY (116): the day of the week, 1 on Sunday to 7 on Saturday
[[nodiscard]] float WeekDay(const std::tm& now);

/// GET_REAL_MONTH: 1 in January to 12 in December
[[nodiscard]] float Month(const std::tm& now);

/// GET_REAL_YEAR: the year in full
[[nodiscard]] float Year(const std::tm& now);
} // namespace openblack::script::real_time
