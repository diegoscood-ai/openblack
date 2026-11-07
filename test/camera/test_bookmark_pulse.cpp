/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The bookmarks' shared pulse (src/Camera/BookmarkPulse.h) against the original's numbers, from the exe oracle

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Camera/BookmarkPulse.h"

using namespace openblack;

namespace
{
/// The pulse time before a drawn frame, the frame's game ms, and the original's time, width and height after it (the
/// floats as their bits)
struct PulseCase
{
	int32_t timeMs;
	uint32_t frameMs;
	int32_t timeAfter;
	uint32_t width;
	uint32_t height;
};

constexpr std::array<PulseCase, 18> k_PulseCases {{
    {0, 0, 0, 0x3F99999Au, 0x3F812778u},
    {0, 1, 1, 0x3F999970u, 0x3F80EAD4u},
    {0, 13, 13, 0x3F997E47u, 0x3F7BFDEFu},
    {0, 100, 100, 0x3F9355D2u, 0x3F49AC4Au},
    {0, 250, 250, 0x3F683826u, 0x3EB67E91u},
    {0, 500, 500, 0x3E4CCCD0u, 0x3F49AC4Eu},
    {0, 999, 999, 0x3F999970u, 0x3F8163D9u},
    {0, 1000, 1000, 0x3F99999Au, 0x3F812776u},
    {0, 1234, 1234, 0x3F7115F9u, 0x3ECFCD36u},
    {0, 1500, 1500, 0x3E4CCCEEu, 0x3F49AC4Fu},
    {0, 1800, 1800, 0x3F81277Au, 0x3F99999Au},
    {0, 2000, 2000, 0x3F99999Au, 0x3F812778u},
    {1990, 30, 20, 0x3F9958F1u, 0x3F78738Eu},
    {2000, 0, 2000, 0x3F99999Au, 0x3F812778u},
    {2000, 1, 1, 0x3F999970u, 0x3F80EAD4u},
    {1999, 1, 2000, 0x3F99999Au, 0x3F812778u},
    {1500, 1700, 1200, 0x3F812776u, 0x3F024EEAu},
    {7, 4001, 8, 0x3F998F40u, 0x3F7E7627u},
}};
} // namespace

// The time moves on by the frame's ms and back into the 2 s period; the width and height are |cos| plus 0.2, the height
// 200 ms ahead, bit for bit as the original's
TEST(BookmarkPulse, TheWidthAndHeightAreTheOriginals)
{
	for (const auto& c : k_PulseCases)
	{
		const auto pulse = bookmark_pulse::Step({.timeMs = c.timeMs}, c.frameMs);
		EXPECT_EQ(pulse.timeMs, c.timeAfter) << c.timeMs << " + " << c.frameMs;
		EXPECT_EQ(std::bit_cast<uint32_t>(pulse.width), c.width) << c.timeMs << " + " << c.frameMs;
		EXPECT_EQ(std::bit_cast<uint32_t>(pulse.height), c.height) << c.timeMs << " + " << c.frameMs;
	}
}

// Paused, the frame's game ms are 0 and the pulse stands still
TEST(BookmarkPulse, APausedFrameLeavesThePulse)
{
	const auto pulse = bookmark_pulse::Step({.timeMs = 0}, 1234);
	const auto paused = bookmark_pulse::Step(pulse, 0);
	EXPECT_EQ(paused.timeMs, pulse.timeMs);
	EXPECT_EQ(std::bit_cast<uint32_t>(paused.width), std::bit_cast<uint32_t>(pulse.width));
	EXPECT_EQ(std::bit_cast<uint32_t>(paused.height), std::bit_cast<uint32_t>(pulse.height));
}

// Nothing goes under the minimum, nor over the minimum plus 1
TEST(BookmarkPulse, TheWaveStaysBetweenItsMinimumAndOneMore)
{
	for (int32_t time = 0; time <= bookmark_pulse::k_PeriodMs; ++time)
	{
		const float wave = bookmark_pulse::Wave(time);
		EXPECT_GE(wave, bookmark_pulse::k_Minimum - 1e-6f) << time;
		EXPECT_LE(wave, bookmark_pulse::k_Minimum + 1.0f + 1e-6f) << time;
	}
}

// A marker is the pulse's width wide and its stretch (height / width) times that high: at 13 ms the original draws a
// half height one bit under the pulse's height
TEST(BookmarkPulse, TheMarkersHalfSizeIsTheOriginals)
{
	const auto pulse = bookmark_pulse::Step({.timeMs = 0}, 13);
	const auto halfSize = bookmark_pulse::MarkerHalfSize(pulse);
	EXPECT_EQ(std::bit_cast<uint32_t>(halfSize.x), 0x3F997E47u);
	EXPECT_EQ(std::bit_cast<uint32_t>(halfSize.y), 0x3F7BFDEEu);
	const auto atHundred = bookmark_pulse::MarkerHalfSize(bookmark_pulse::Step({.timeMs = 0}, 100));
	EXPECT_EQ(std::bit_cast<uint32_t>(atHundred.x), 0x3F9355D2u);
	EXPECT_EQ(std::bit_cast<uint32_t>(atHundred.y), 0x3F49AC4Au);
}
