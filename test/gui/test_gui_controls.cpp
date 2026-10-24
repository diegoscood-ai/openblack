/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <chrono>

#include <gtest/gtest.h>

#include "Gui/Controls.h"

using openblack::gui::IsCaretShown;

TEST(EditBoxCaret, ShownOnTheOddFifthsOfASecondOfTheTickCount)
{
	EXPECT_FALSE(IsCaretShown(0));
	EXPECT_FALSE(IsCaretShown(199));
	EXPECT_TRUE(IsCaretShown(200));
	EXPECT_TRUE(IsCaretShown(399));
	EXPECT_FALSE(IsCaretShown(400));
	EXPECT_FALSE(IsCaretShown(599));
	EXPECT_TRUE(IsCaretShown(600));
	EXPECT_TRUE(IsCaretShown(1000 * 3600 + 200));
	EXPECT_FALSE(IsCaretShown(1000 * 3600 + 400));
	// The tick count wraps at 2^32: 4294967295 / 200 = 21474836, even
	EXPECT_FALSE(IsCaretShown(UINT32_MAX));
}
