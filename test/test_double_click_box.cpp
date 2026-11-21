/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Input/DoubleClickBox.h"

using namespace openblack::input::double_click;

TEST(DoubleClickBox, TheSystemsDefaultBoxIsARadiusOfTwoPixels)
{
	// Windows' default box, 4 by 4: a second press up to 2 pixels off on each axis is a double click
	EXPECT_EQ(RadiusForBox(k_DefaultBoxSize, k_DefaultBoxSize), 2);
}

TEST(DoubleClickBox, ABoxOfOtherSidesTakesHalfItsNarrowerSide)
{
	EXPECT_EQ(RadiusForBox(8, 8), 4);
	EXPECT_EQ(RadiusForBox(10, 6), 3);
	EXPECT_EQ(RadiusForBox(5, 5), 2);
	// a system with no box makes every second press too far
	EXPECT_EQ(RadiusForBox(0, 0), 0);
	EXPECT_EQ(RadiusForBox(-1, 4), 0);
}

TEST(DoubleClickBox, AQuickRowsPressesArePairedTwoByTwo)
{
	// a row of quick presses as SDL counts it: the second of each pair is the system's double click
	EXPECT_FALSE(IsDoubleClick(1));
	EXPECT_TRUE(IsDoubleClick(2));
	EXPECT_FALSE(IsDoubleClick(3));
	EXPECT_TRUE(IsDoubleClick(4));
	EXPECT_FALSE(IsDoubleClick(5));
	EXPECT_TRUE(IsDoubleClick(6));
}

TEST(DoubleClickBox, AnEventWithNoClickCountIsNoDoubleClick)
{
	EXPECT_FALSE(IsDoubleClick(0));
}
