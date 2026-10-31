/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The moon's date read for its phase while it shows, at most once every two seconds of the machine's clock, with fake
// dates and ticks. docs/wiki/world/sky.md#sky-sun-moon-and-clouds-original

#include <cstdint>

#include <gtest/gtest.h>

#include "3D/SkyFrame.h"
#include "ECS/Components/Sky.h"
#include "Graphics/Moon.h"

using namespace openblack;
using ecs::components::Moon;

namespace
{
// 2026-01-01
constexpr int64_t k_Date = 1767225600;
} // namespace

TEST(SkyFrame, TheMoonStartsAsANewMoonWithNoDateRead)
{
	const Moon moon;
	EXPECT_EQ(moon.phase, 0.0f);
	EXPECT_EQ(moon.date, 0);
	EXPECT_EQ(moon.dateReadAt, 0u);
}

TEST(SkyFrame, TheFirstShowReadsTheDate)
{
	// Within the first two seconds of the machine's clock, and after them
	for (const uint32_t ticks : {0u, 1500u, 90000u})
	{
		Moon moon;
		sky_frame::TakeMoonPhase(moon, k_Date, ticks);
		EXPECT_EQ(moon.date, k_Date) << ticks;
		EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date)) << ticks;
	}
}

TEST(SkyFrame, TheDateIsReadAtMostOnceEveryTwoSeconds)
{
	Moon moon;
	constexpr int64_t k_Later = k_Date + (7 * 86400);
	sky_frame::TakeMoonPhase(moon, k_Date, 5000);
	EXPECT_EQ(moon.date, k_Date);
	EXPECT_EQ(moon.dateReadAt, 5000u);

	// Two seconds exactly is not yet more than two seconds: the date read is kept
	sky_frame::TakeMoonPhase(moon, k_Later, 7000);
	EXPECT_EQ(moon.date, k_Date);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date));

	sky_frame::TakeMoonPhase(moon, k_Later, 7001);
	EXPECT_EQ(moon.date, k_Later);
	EXPECT_EQ(moon.dateReadAt, 7001u);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Later));
}

TEST(SkyFrame, TheMachinesClockWrapsBetweenReads)
{
	Moon moon;
	moon.dateReadAt = 0xFFFFFF00u;
	moon.date = k_Date;
	// 0x100 + 1000 milliseconds after the last read, across the wrap: the date is kept
	sky_frame::TakeMoonPhase(moon, k_Date + 86400, 1000);
	EXPECT_EQ(moon.date, k_Date);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Date));
}

TEST(SkyFrame, ADateOverrideGivesThePhase)
{
	Moon moon;
	EXPECT_FALSE(moon.dateOverride.has_value());
	constexpr int64_t k_Override = k_Date + (12 * 86400);
	moon.dateOverride = k_Override;
	sky_frame::TakeMoonPhase(moon, k_Date, 0);
	EXPECT_EQ(sky_frame::MoonDate(moon), k_Override);
	EXPECT_EQ(moon.phase, graphics::moon::Phase(k_Override));
	// The computer's date is still read and kept for when the override goes
	EXPECT_EQ(moon.date, k_Date);
	moon.dateOverride.reset();
	EXPECT_EQ(sky_frame::MoonDate(moon), k_Date);
}
