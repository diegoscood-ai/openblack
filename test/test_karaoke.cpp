/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Help/Karaoke.h"

// The song's karaoke box, pinned to the original's numbers: its word draw run frame by frame with fixed word widths,
// the box it hands the glow panel and its three Zoomers read after each frame.

using namespace openblack::help::karaoke;

namespace
{
struct Frame
{
	float x;
	float y;
	float width;
	uint32_t ms;
	Box box;
	std::array<uint32_t, 3> value; ///< x, y, width
	std::array<uint32_t, 3> speed;
};

uint32_t Bits(float f)
{
	return std::bit_cast<uint32_t>(f);
}

void Check(Highlight& highlight, const Frame& f, float lineHeight, std::array<uint8_t, 4> rgba)
{
	const Box box = highlight.Follow(f.x, f.y, f.width, lineHeight, rgba, f.ms);
	EXPECT_EQ(box.left, f.box.left);
	EXPECT_EQ(box.top, f.box.top);
	EXPECT_EQ(box.right, f.box.right);
	EXPECT_EQ(box.bottom, f.box.bottom);
	EXPECT_EQ(box.rgba, f.box.rgba);
	EXPECT_EQ(Bits(highlight.X().value), f.value[0]);
	EXPECT_EQ(Bits(highlight.Y().value), f.value[1]);
	EXPECT_EQ(Bits(highlight.Width().value), f.value[2]);
	EXPECT_EQ(Bits(highlight.X().speed), f.speed[0]);
	EXPECT_EQ(Bits(highlight.Y().speed), f.speed[1]);
	EXPECT_EQ(Bits(highlight.Width().speed), f.speed[2]);
}
} // namespace

TEST(Karaoke, TheLanguagesThatFollowTheSong)
{
	const std::array<bool, 16> expected {true,  true, true,  true,  true, true,  true,  true,
	                                     false, true, false, false, true, false, false, false};
	for (uint32_t language = 0; language < expected.size(); ++language)
	{
		EXPECT_EQ(UsesBouncingBall(language), expected.at(language)) << language;
	}
	EXPECT_FALSE(UsesBouncingBall(0xFFFFFFFFu));
}

TEST(Karaoke, OnlyTheVersesAndTheNewestTextsBeatWord)
{
	EXPECT_FALSE(IsVerseMusic(50));
	EXPECT_TRUE(IsVerseMusic(51));
	EXPECT_TRUE(IsVerseMusic(53));
	EXPECT_FALSE(IsVerseMusic(54));
	EXPECT_TRUE(IsSungWord(true, true, true, 2, 2));
	EXPECT_FALSE(IsSungWord(true, true, true, 1, 2));
	EXPECT_FALSE(IsSungWord(false, true, true, 2, 2));
	EXPECT_FALSE(IsSungWord(true, false, true, 2, 2));
	EXPECT_FALSE(IsSungWord(true, true, false, 2, 2));
	// No beat yet: the first word's index is 1
	EXPECT_FALSE(IsSungWord(true, true, true, 1, 0));
}

TEST(Karaoke, TheBoxStartsAtTheBottomLeftWithNoWidth)
{
	const Highlight highlight(480);
	EXPECT_EQ(highlight.X().value, 0.0f);
	EXPECT_EQ(highlight.Y().value, 480.0f);
	EXPECT_EQ(highlight.Y().destination, 480.0f);
	EXPECT_EQ(highlight.Y().startValue, 480.0f);
	EXPECT_EQ(highlight.Width().value, 0.0f);
}

TEST(Karaoke, TheBoxGlidesFromWordToWordAsTheOriginals)
{
	// Words 40 wide at 180, 260 and 340 on the line at 400, line height 16, the word's alpha 201
	const std::array<uint8_t, 4> rgba {0x11, 0x22, 0x33, 201};
	const std::array<uint8_t, 4> half {0x11, 0x22, 0x33, 100};
	const std::array<Frame, 5> frames {{
	    {180.0f,
	     400.0f,
	     40.0f,
	     33,
	     {28, 467, 34, 483, half},
	     {0x41E04B36, 0x43E9C506, 0x40C75F4C},
	     {0x44B768E9, 0xC42307EB, 0x43A307EB}},
	    {180.0f,
	     400.0f,
	     40.0f,
	     33,
	     {78, 445, 95, 461, half},
	     {0x429C28BA, 0x43DEA625, 0x418ACEDE},
	     {0x44BB7633, 0xC426A1F6, 0x43A6A1F4}},
	    {260.0f,
	     400.0f,
	     40.0f,
	     50,
	     {162, 418, 193, 434, half},
	     {0x43229B0C, 0x43D15D76, 0x41F51450},
	     {0x44D0CA2A, 0xC3C775BF, 0x434775B8}},
	    {260.0f,
	     400.0f,
	     40.0f,
	     100,
	     {251, 400, 291, 416, half},
	     {0x437BDCEA, 0x43C870FE, 0x421E3C09},
	     {0x439AA328, 0xC222E0B8, 0x41A2E0D8}},
	    // A frame past the step's time lands on the word
	    {340.0f, 400.0f, 40.0f, 200, {340, 400, 380, 416, half}, {0x43AA0000, 0x43C80000, 0x42200000}, {0, 0, 0}},
	}};
	Highlight highlight(480);
	for (const Frame& f : frames)
	{
		Check(highlight, f, 16.0f, rgba);
	}
}

TEST(Karaoke, FractionalLinesAndAFrameOfNoTime)
{
	// Words 37.25 wide at 271, 345, 419 and 493 on the line at 404.7, line height 15.3, a 600 high screen
	const std::array<uint8_t, 4> rgba {0x11, 0x22, 0x33, 255};
	const std::array<uint8_t, 4> half {0x11, 0x22, 0x33, 127};
	const float y = 401.0f + 0.0f + 3.7f;
	const std::array<Frame, 5> frames {{
	    {271.0f,
	     y,
	     37.25f,
	     17,
	     {12, 590, 14, 606, half},
	     {0x414BDE94, 0x4413B450, 0x3FE02E61},
	     {0x44AEE6ED, 0xC47C175F, 0x434053E4}},
	    {345.0f,
	     y,
	     37.25f,
	     16,
	     {43, 570, 49, 586, half},
	     {0x422E7C0E, 0x440EB359, 0x40B234B1},
	     {0x4514EFF9, 0xC4B5E00F, 0x438AC204}},
	    {345.0f,
	     y,
	     37.25f,
	     0,
	     {43, 570, 49, 586, half},
	     {0x422E7C0E, 0x440EB359, 0x40B234B1},
	     {0x4514EFF9, 0xC4B5E00F, 0x438AC204}},
	    {493.0f, y, 37.25f, 500, {493, 404, 530, 420, half}, {0x43F68000, 0x43CA599A, 0x42150000}, {0, 0, 0}},
	    {271.0f, y, 37.25f, 40, {445, 404, 482, 420, half}, {0x43DE8B3C, 0x43CA599A, 0x42150000}, {0xC4F8B26B, 0, 0}},
	}};
	Highlight highlight(600);
	for (const Frame& f : frames)
	{
		Check(highlight, f, 15.3f, rgba);
	}
}
