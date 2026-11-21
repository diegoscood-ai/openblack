/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// string_utils::WidenWestern: a script's own words widened as the game widens them, through the Western code page
// (docs/wiki/interface/screen-and-readouts.md#the-scripts-screen-texts)

#include <string>

#include <gtest/gtest.h>

#include "Common/StringUtils.h"

using openblack::string_utils::WidenWestern;

TEST(WidenWestern, PlainTextAndTheUpperHalfKeepTheirValues)
{
	EXPECT_EQ(WidenWestern("Game Over"), u"Game Over");
	EXPECT_EQ(WidenWestern("\x7F\xA0\xE9\xFF"), u"\x007F\x00A0\x00E9\x00FF");
	EXPECT_EQ(WidenWestern(""), u"");
}

TEST(WidenWestern, TheBytesFrom0x80To0x9FTakeTheWesternPagesCharacters)
{
	EXPECT_EQ(WidenWestern("\x80"), u"\x20AC");
	EXPECT_EQ(WidenWestern("\x85\x91\x92\x93\x94\x96\x97"), u"\x2026\x2018\x2019\x201C\x201D\x2013\x2014");
	EXPECT_EQ(WidenWestern("\x8A\x8C\x9A\x9C\x9F"), u"\x0160\x0152\x0161\x0153\x0178");
	EXPECT_EQ(WidenWestern("\x99"), u"\x2122");
	// the five the page leaves unset keep their own value
	EXPECT_EQ(WidenWestern("\x81\x8D\x8F\x90\x9D"), u"\x0081\x008D\x008F\x0090\x009D");
}

TEST(WidenWestern, ATextTooLongForTheConversionsBufferGivesNothing)
{
	EXPECT_EQ(WidenWestern(std::string(2046, 'a')).size(), 2046u);
	EXPECT_TRUE(WidenWestern(std::string(2047, 'a')).empty());
}
