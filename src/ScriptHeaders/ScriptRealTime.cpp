/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptRealTime.h"

#include <bit>

namespace openblack::script::real_time
{
namespace
{
/// One hour over 3600 seconds, as the original's float constant
constexpr float k_HourPerSecond = std::bit_cast<float>(0x3991A2B4U);
/// The year std::tm counts from
constexpr float k_FirstYear = 1900.0f;
} // namespace

std::tm Now()
{
	const std::time_t now = std::time(nullptr);
	std::tm local {};
#if defined(_WIN32)
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	return local;
}

float Time(const std::tm& now)
{
	// The seconds into the hour as a float, times the constant, plus the hour: each step a float
	const float seconds = static_cast<float>((now.tm_min * 60) + now.tm_sec);
	const float fraction = seconds * k_HourPerSecond;
	return fraction + static_cast<float>(now.tm_hour);
}

float MonthDay(const std::tm& now)
{
	return static_cast<float>(now.tm_mday);
}

float WeekDay(const std::tm& now)
{
	return static_cast<float>(now.tm_wday) + 1.0f;
}

float Month(const std::tm& now)
{
	return static_cast<float>(now.tm_mon) + 1.0f;
}

float Year(const std::tm& now)
{
	return static_cast<float>(now.tm_year) + k_FirstYear;
}
} // namespace openblack::script::real_time
