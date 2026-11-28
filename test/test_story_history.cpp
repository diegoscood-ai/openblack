/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Help/StoryHistory.h"

using namespace openblack::help;

TEST(StoryHistory, EachNarratorHasItsCaption)
{
	EXPECT_EQ(StoryCaption(2), "HELP_TEXT_DIALOG_ADDITION_131");
	EXPECT_EQ(StoryCaption(3), "HELP_TEXT_DIALOG_ADDITION_128");
	EXPECT_EQ(StoryCaption(7), "HELP_TEXT_DIALOG_ADDITION_79");
	EXPECT_EQ(StoryCaption(11), "HELP_TEXT_DIALOG_ADDITION_15");
	EXPECT_EQ(StoryCaption(12), "HELP_TEXT_DIALOG_ADDITION_134");
	// No narrator, the default narrator and the animals all give "This was said:"
	EXPECT_EQ(StoryCaption(0), "HELP_TEXT_DIALOG_ADDITION_135");
	EXPECT_EQ(StoryCaption(1), "HELP_TEXT_DIALOG_ADDITION_135");
	EXPECT_EQ(StoryCaption(15), "HELP_TEXT_DIALOG_ADDITION_135");
}

TEST(StoryHistory, TheFirstLineShownStaysInTheHistory)
{
	EXPECT_EQ(ClampStoryStart(3, 10), 3);
	EXPECT_EQ(ClampStoryStart(10, 10), 9);
	EXPECT_EQ(ClampStoryStart(-1, 10), 0);
	// An empty history starts at the newest line
	EXPECT_EQ(ClampStoryStart(4, 0), 0);
}
