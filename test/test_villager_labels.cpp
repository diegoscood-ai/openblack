/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Gui/VillagerLabels.h"

using namespace openblack::gui::villager_labels;

namespace
{
/// Every character ten pixels wide at full size, whatever the line height
float TenPixelsEach(std::u16string_view text, float /*lineHeight*/)
{
	return 10.0f * static_cast<float>(text.size());
}

Request At(int x, int y, std::u16string text)
{
	return {.pixel = {x, y}, .depth = 10.0f, .size = 1.0f, .text = std::move(text)};
}
} // namespace

TEST(VillagerLabels, NameDisplayGrowsAndShrinksByTheGameTime)
{
	EXPECT_FLOAT_EQ(StepNameFade(0.0f, true, 100), 0.3f);
	EXPECT_FLOAT_EQ(StepNameFade(0.9f, true, 100), 1.0f);
	EXPECT_FLOAT_EQ(StepNameFade(1.0f, false, 100), 0.7f);
	EXPECT_FLOAT_EQ(StepNameFade(0.1f, false, 100), 0.0f);
	// At its target it stays there
	EXPECT_FLOAT_EQ(StepNameFade(1.0f, true, 16), 1.0f);
	EXPECT_FLOAT_EQ(StepNameFade(0.0f, false, 16), 0.0f);
}

TEST(VillagerLabels, NameDisplayTakesAThirdOfASecondOfGameTimeToGrowIn)
{
	// 0.003 a millisecond: 333 milliseconds are not quite enough, 334 are
	float fade = 0.0f;
	for (int frame = 0; frame < 111; ++frame)
	{
		fade = StepNameFadeFrame(fade, true, 3, true).next;
	}
	EXPECT_LT(fade, 1.0f);
	EXPECT_NEAR(fade, 0.999f, 1e-4f);
	fade = StepNameFadeFrame(fade, true, 1, true).next;
	EXPECT_FLOAT_EQ(fade, 1.0f);
	// The step is linear, the same in each direction
	EXPECT_FLOAT_EQ(StepNameFadeFrame(0.5f, true, 16, true).next, 0.548f);
	EXPECT_FLOAT_EQ(StepNameFadeFrame(0.5f, false, 16, true).next, 0.452f);
}

TEST(VillagerLabels, FrameNamesUseTheNameDisplayFromBeforeTheStep)
{
	const auto frame = StepNameFadeFrame(0.0f, true, 16, true);
	EXPECT_FLOAT_EQ(frame.drawn, 0.0f);
	EXPECT_FLOAT_EQ(frame.next, 0.048f);
	// Thus the first frame after the key shows no name, and the next shows it small
	EXPECT_FALSE(NameSize({.named = true, .nameFade = frame.drawn}).has_value());
	const auto next = StepNameFadeFrame(frame.next, true, 16, true);
	EXPECT_FLOAT_EQ(next.drawn, 0.048f);
	const auto size = NameSize({.named = true, .nameFade = next.drawn});
	ASSERT_TRUE(size.has_value());
	EXPECT_FLOAT_EQ(*size, 0.048f);
}

TEST(VillagerLabels, NameDisplayStandsStillWhenTheLandIsNotDrawnOrTheGameIsPaused)
{
	// In the temple, under a video or behind the falling spell's film
	auto frame = StepNameFadeFrame(0.4f, true, 16, false);
	EXPECT_FLOAT_EQ(frame.drawn, 0.4f);
	EXPECT_FLOAT_EQ(frame.next, 0.4f);
	frame = StepNameFadeFrame(0.4f, false, 16, false);
	EXPECT_FLOAT_EQ(frame.next, 0.4f);
	// A paused frame has no game time
	frame = StepNameFadeFrame(0.4f, true, 0, true);
	EXPECT_FLOAT_EQ(frame.next, 0.4f);
}

TEST(VillagerLabels, OnlyNamedVillagersNoScriptHoldsOrMadeShowAName)
{
	EXPECT_FLOAT_EQ(NameSize({.named = true, .nameFade = 0.4f}).value_or(-1.0f), 0.4f);
	EXPECT_FALSE(NameSize({.named = false, .nameFade = 1.0f}).has_value());
	// The display must be past a hundredth
	EXPECT_FALSE(NameSize({.named = true, .nameFade = 0.01f}).has_value());
	EXPECT_TRUE(NameSize({.named = true, .nameFade = 0.0101f}).has_value());
	EXPECT_FALSE(NameSize({.named = true, .inScript = true, .nameFade = 1.0f}).has_value());
	EXPECT_FALSE(NameSize({.named = true, .nameHidden = true, .nameFade = 1.0f}).has_value());
	EXPECT_FALSE(NameSize({.named = true, .speaking = true, .nameFade = 1.0f}).has_value());
}

TEST(VillagerLabels, ColourGoesHalfWayToWhite)
{
	// No player: white stays white
	EXPECT_EQ(NameColour(std::nullopt), glm::u8vec3(255));
	// The first player's red, 255,70,70, goes half way to white, rounding down
	EXPECT_EQ(NameColour(glm::u8vec3(255, 70, 70)), glm::u8vec3(255, 162, 162));
	// A colour all but black (the neutral player's) is white
	EXPECT_EQ(NameColour(glm::u8vec3(9, 9, 9)), glm::u8vec3(255));
	EXPECT_EQ(NameColour(glm::u8vec3(10, 0, 0)), glm::u8vec3(132, 127, 127));
}

TEST(VillagerLabels, LabelStandsOverItsPoint)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 300, u"abc\nde"), 800, TenPixelsEach));
	const auto& label = board.GetLabels().front();
	// 30 for the widest line and 6 more, centred on the point
	EXPECT_FLOAT_EQ(label.left, 382.0f);
	EXPECT_FLOAT_EQ(label.right, 418.0f);
	EXPECT_FLOAT_EQ(label.bottom, 250.0f);
	// Two lines of 16 and 4 more
	EXPECT_FLOAT_EQ(label.top, 214.0f);
	EXPECT_FLOAT_EQ(label.inset, 2.0f);
	ASSERT_EQ(label.lines.size(), 2u);
	EXPECT_EQ(label.lines[1], u"de");
	EXPECT_EQ(label.boxAlpha, 120);
	EXPECT_EQ(label.wordsAlpha, 255);
}

TEST(VillagerLabels, LabelsKeepOnTheScreenAcross)
{
	Board board;
	ASSERT_TRUE(board.Add(At(5, 300, u"abcd"), 800, TenPixelsEach));
	EXPECT_FLOAT_EQ(board.GetLabels().front().left, 0.0f);
	ASSERT_TRUE(board.Add(At(799, 100, u"abcd"), 800, TenPixelsEach));
	EXPECT_FLOAT_EQ(board.GetLabels().back().right, 800.0f);
	EXPECT_FLOAT_EQ(board.GetLabels().back().left, 754.0f);
	// Wider than the screen it is cut to half the screen, in whole pixels
	Board narrow;
	ASSERT_TRUE(narrow.Add(At(50, 300, u"abcdefghijklmnop"), 101, TenPixelsEach));
	EXPECT_FLOAT_EQ(narrow.GetLabels().front().right - narrow.GetLabels().front().left, 50.0f);
}

TEST(VillagerLabels, LaterLabelsStackOverEarlierOnes)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 300, u"abc"), 800, TenPixelsEach));
	ASSERT_TRUE(board.Add(At(405, 305, u"abc"), 800, TenPixelsEach));
	const auto& labels = board.GetLabels();
	ASSERT_EQ(labels.size(), 2u);
	// The first placed is the left one; the second sits a pixel above it
	EXPECT_FLOAT_EQ(labels[0].top, 230.0f);
	EXPECT_FLOAT_EQ(labels[1].bottom, 229.0f);
	EXPECT_FLOAT_EQ(labels[1].top, 209.0f);
}

TEST(VillagerLabels, LabelsAreKeptFromLeftToRight)
{
	Board board;
	ASSERT_TRUE(board.Add(At(600, 300, u"b"), 800, TenPixelsEach));
	ASSERT_TRUE(board.Add(At(100, 300, u"a"), 800, TenPixelsEach));
	ASSERT_EQ(board.GetLabels().size(), 2u);
	EXPECT_EQ(board.GetLabels()[0].lines[0], u"a");
	EXPECT_EQ(board.GetLabels()[1].lines[0], u"b");
}

TEST(VillagerLabels, LabelPushedOffTheTopIsDropped)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 90, u"abc"), 800, TenPixelsEach));
	EXPECT_FALSE(board.Add(At(400, 90, u"abc"), 800, TenPixelsEach));
	EXPECT_EQ(board.GetLabels().size(), 1u);
	EXPECT_FALSE(board.Add(At(400, 60, u"abc"), 800, TenPixelsEach));
}

TEST(VillagerLabels, SmallLabelsAreNotShownAndAGrowingOneIsSmaller)
{
	Board board;
	auto request = At(400, 300, u"abc");
	request.size = 0.005f;
	EXPECT_FALSE(board.Add(request, 800, TenPixelsEach));
	request.size = 0.5f;
	ASSERT_TRUE(board.Add(request, 800, TenPixelsEach));
	const auto& label = board.GetLabels().front();
	EXPECT_FLOAT_EQ(label.lineHeight, 8.0f);
	EXPECT_FLOAT_EQ(label.inset, 1.0f);
	// The alphas are cut to a whole number
	EXPECT_EQ(label.boxAlpha, 60);
	EXPECT_EQ(label.wordsAlpha, 127);
	// Bigger than full size it is drawn at full size
	request.size = 3.0f;
	request.pixel = {100, 500};
	ASSERT_TRUE(board.Add(request, 800, TenPixelsEach));
	EXPECT_FLOAT_EQ(board.GetLabels().front().lineHeight, 16.0f);
}

TEST(VillagerLabels, AtMostEightLines)
{
	Board board;
	ASSERT_TRUE(board.Add(At(400, 500, u"1\n2\n3\n4\n5\n6\n7\n8\n9"), 800, TenPixelsEach));
	const auto& lines = board.GetLabels().front().lines;
	ASSERT_EQ(lines.size(), 8u);
	EXPECT_EQ(lines.back(), u"8\n9");
}

TEST(VillagerLabels, PointerReachesThePointAndKeepsItsSize)
{
	Board board;
	auto small = At(200, 300, u"abc");
	small.size = 0.25f;
	ASSERT_TRUE(board.Add(small, 800, TenPixelsEach));
	ASSERT_TRUE(board.Add(At(600, 300, u"abc"), 800, TenPixelsEach));
	const auto& labels = board.GetLabels();
	ASSERT_EQ(labels.size(), 2u);
	EXPECT_EQ(labels[0].boxAlpha, 30);
	const auto full = PointerShape(labels[1]);
	// The stalk from the box's bottom down to 16 pixels above the point, 6 wide
	EXPECT_EQ(full[0].x0, 597);
	EXPECT_EQ(full[0].x1, 603);
	EXPECT_EQ(full[0].y0, 250);
	EXPECT_EQ(full[0].y1, 284);
	EXPECT_FLOAT_EQ(full[0].uv0.x, 0.53125f);
	EXPECT_FLOAT_EQ(full[0].uv1.y, 0.05f);
	// The head, 12 wide, down to the point
	EXPECT_EQ(full[1].x0, 594);
	EXPECT_EQ(full[1].x1, 606);
	EXPECT_EQ(full[1].y0, 284);
	EXPECT_EQ(full[1].y1, 300);
	EXPECT_FLOAT_EQ(full[1].uv1.x, 0.625f);
	// The same at a quarter of the size
	const auto quarter = PointerShape(labels[0]);
	EXPECT_EQ(quarter[1].x1 - quarter[1].x0, 12);
	EXPECT_EQ(quarter[1].y1 - quarter[1].y0, 16);
}

TEST(VillagerLabels, ALabelKeepsItsPointAndDepth)
{
	Board board;
	auto request = At(400, 300, u"abc");
	request.point = {1.0f, 2.0f, 3.0f};
	request.depth = 42.0f;
	ASSERT_TRUE(board.Add(request, 800, TenPixelsEach));
	EXPECT_EQ(board.GetLabels().front().point, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_FLOAT_EQ(board.GetLabels().front().depth, 42.0f);
}
