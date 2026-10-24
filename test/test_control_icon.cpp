/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <string>

#include <gtest/gtest.h>

#include "Help/ClickCue.h"
#include "Help/ControlIcon.h"

using namespace openblack::help;

TEST(ControlIcon, TheActionsFromSixToFifteenAreKeys)
{
	EXPECT_FALSE(control_icon::IsKeyAction(5));
	EXPECT_TRUE(control_icon::IsKeyAction(6));
	EXPECT_TRUE(control_icon::IsKeyAction(15));
	EXPECT_FALSE(control_icon::IsKeyAction(16));
	EXPECT_FALSE(control_icon::IsKeyAction(-1));
}

TEST(ControlIcon, TheSecondKeyFollowsAPlusAndTheOthersASlash)
{
	std::u16string text;
	int count = 0;
	control_icon::AppendKeyName(text, count, u"Shift");
	EXPECT_EQ(text, u"Shift");
	control_icon::AppendKeyName(text, count, u"A");
	EXPECT_EQ(text, u"Shift + A");
	control_icon::AppendKeyName(text, count, u"B");
	EXPECT_EQ(text, u"Shift + A / B");
	EXPECT_EQ(count, 3);
}

TEST(ControlIcon, AKeySitsInTheBoxsTopThirdAndAMouseOnTheCuesRow)
{
	// A box from 400 to 459: the icon is (459 - 400 + 1) / 3 = 20 high
	EXPECT_EQ(click_cue::CueSize(400, 459), 20);
	EXPECT_EQ(control_icon::KeyIconRow(400, 459), 410);
	EXPECT_EQ(click_cue::MiddleRow(400, 459), 450);
	// Cut towards zero: 62 high gives 20, and half of it 10
	EXPECT_EQ(control_icon::KeyIconRow(0, 61), 10);
}
