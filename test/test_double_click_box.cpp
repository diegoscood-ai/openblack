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
