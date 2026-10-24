/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frame counter's corner readout: it shows while its setting is on, and never in a run of the checks.

#include <gtest/gtest.h>

#include "Debug/FrameCounter.h"

using openblack::debug::frame_counter::IsCheckRun;
using openblack::debug::frame_counter::IsVisible;

TEST(FrameCounter, APersonPlayingIsNotACheckRun)
{
	EXPECT_FALSE(IsCheckRun(false, false, 0));
}

TEST(FrameCounter, EachCheckSignMakesACheckRun)
{
	EXPECT_TRUE(IsCheckRun(true, false, 0));
	EXPECT_TRUE(IsCheckRun(false, true, 0));
	EXPECT_TRUE(IsCheckRun(false, false, 900));
	EXPECT_TRUE(IsCheckRun(true, true, 900));
}

TEST(FrameCounter, ShowsOnlyWithTheSettingOutsideTheChecks)
{
	EXPECT_TRUE(IsVisible(true, false));
	EXPECT_FALSE(IsVisible(false, false));
	EXPECT_FALSE(IsVisible(true, true));
	EXPECT_FALSE(IsVisible(false, true));
}

static_assert(IsVisible(true, IsCheckRun(false, false, 0)));
static_assert(!IsVisible(true, IsCheckRun(true, false, 0)));
