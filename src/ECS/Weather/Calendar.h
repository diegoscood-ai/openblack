/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

// The date part of GGameInfo (GameInfo.cpp of the original), what the climate reads: a game year lasts 36000 game turns
// (one hour at 10 turns per second), so a day is 98.56 turns (9.9 s). The land starts on 5 May 1998 at 18:05:30
// (GGameInfo::GGameInfo 0x557730; the map commands that change it, GSetup 0x714D57 / 0x714D81, are not used by the
// original lands). This is not the visual time of day (DayNightClock), which runs on its own.

namespace openblack::weather::calendar
{
/// GGameInfo +0x10: game turns per year (36000)
constexpr float k_TurnsPerYear = 36000.0f;
constexpr float k_SecondsInDay = 86400.0f; ///< _SecondsInDay 0x8DF8E0
constexpr float k_NumDaysInYear = 365.25f; ///< _NumDaysInYear 0x8DF8DC

/// GGameInfo::GetDaysFromStart 0x557940: (turn + start) x (+0x18) / 86400, without the whole years of the start date
/// (the day of the year is the same: they are multiples of 365.25 days)
[[nodiscard]] double GetDaysFromStart(uint32_t turn);
/// fmod(GetDaysFromStart, 365.25): what GetSeason, the month and the day of the month start from
[[nodiscard]] float GetDayOfYear(uint32_t turn);
/// fn_00557960: the month 1..12 (the first whose cumulative day count is above the day of the year; 12 past the last)
[[nodiscard]] int32_t GetMonth(uint32_t turn);
/// fn_005579C0: the day of the month from 0 (the day of the year minus the days of the months before)
[[nodiscard]] float GetDayOfMonth(uint32_t turn);
/// GGameInfo::GetSeason 0x557A80: 0 spring (from day 79), 1 summer (171), 2 autumn (263), 3 winter (before 79);
/// the GClimateInfo columns are in this order
[[nodiscard]] uint32_t GetSeason(uint32_t turn);
} // namespace openblack::weather::calendar
