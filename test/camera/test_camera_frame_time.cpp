/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <chrono>

#include <gtest/gtest.h>

#include "Camera/CameraFrameTime.h"

using namespace openblack;
using namespace std::chrono_literals;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}
} // namespace

// The whole milliseconds as a float times the float 0.001, rounded to a float, with no cap
TEST(CameraFrameTime, IsTheWholeMillisecondsTimesTheFloatThousandth)
{
	EXPECT_EQ(Bits(camera_frame::Seconds(0u)), 0u);
	EXPECT_EQ(Bits(camera_frame::Seconds(1u)), 0x3A83126Fu);
	EXPECT_EQ(Bits(camera_frame::Seconds(16u)), 0x3C83126Fu);
	EXPECT_EQ(Bits(camera_frame::Seconds(33u)), 0x3D072B02u);
	EXPECT_EQ(Bits(camera_frame::Seconds(99u)), 0x3DCAC084u);
	EXPECT_EQ(Bits(camera_frame::Seconds(101u)), 0x3DCED917u);
	EXPECT_EQ(Bits(camera_frame::Seconds(250u)), 0x3E800000u);
	EXPECT_EQ(Bits(camera_frame::Seconds(1000u)), 0x3F800000u);
}

// The product rounds above the nearest float to the exact seconds for many frame lengths, so the frame time is not the
// milliseconds divided by 1000
TEST(CameraFrameTime, RoundsAsTheProductNotAsTheQuotient)
{
	EXPECT_EQ(Bits(camera_frame::Seconds(10u)), 0x3C23D70Bu);
	EXPECT_EQ(Bits(0.01f), 0x3C23D70Au);
	EXPECT_EQ(Bits(camera_frame::Seconds(15u)), 0x3C75C290u);
	EXPECT_EQ(Bits(0.015f), 0x3C75C28Fu);
	EXPECT_EQ(Bits(camera_frame::Seconds(20u)), 0x3CA3D70Bu);
	// A power of two times the step keeps its rounding: 16 ms, the checks' fixed step, is the same either way
	EXPECT_EQ(camera_frame::Seconds(16u), std::chrono::duration<float>(16ms).count());
}

// The camera's own time and its zoomers take at most a tenth of a second a frame
TEST(CameraFrameTime, TheCappedTimeIsAtMostATenth)
{
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(0u)), 0u);
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(15u)), 0x3C75C290u);
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(99u)), 0x3DCAC084u);
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(100u)), 0x3DCCCCCDu);
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(101u)), 0x3DCCCCCDu);
	EXPECT_EQ(Bits(camera_frame::CappedSeconds(500u)), 0x3DCCCCCDu);
	EXPECT_EQ(camera_frame::CappedSeconds(100u), camera_frame::k_MaxSeconds);
}

// A step given as a duration counts its whole milliseconds only, and a step that is not forwards counts none
TEST(CameraFrameTime, AStepCountsItsWholeMilliseconds)
{
	EXPECT_EQ(camera_frame::WholeMs(16999us), 16u);
	EXPECT_EQ(camera_frame::WholeMs(17000us), 17u);
	EXPECT_EQ(camera_frame::WholeMs(999us), 0u);
	EXPECT_EQ(camera_frame::WholeMs(-5000us), 0u);
	EXPECT_EQ(camera_frame::Seconds(16999us), camera_frame::Seconds(16u));
	EXPECT_EQ(camera_frame::CappedSeconds(250ms), camera_frame::k_MaxSeconds);
	EXPECT_EQ(camera_frame::Step(16u), 16000us);
	EXPECT_EQ(camera_frame::Seconds(camera_frame::Step(15u)), camera_frame::Seconds(15u));
}

// The step's conversion is a constant expression
static_assert(camera_frame::WholeMs(camera_frame::Step(33u)) == 33u);
static_assert(camera_frame::CappedSeconds(1000u) == camera_frame::k_MaxSeconds);
