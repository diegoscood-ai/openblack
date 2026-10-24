/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The gesture icons at the bottom right of the screen (Magic/Gestures/GestureIcons.h): the list the pass's table makes,
// only again when the table changes, in the order of its types; each icon's box; the fade with the help text; the
// sparkles running along the shape; the symbol's and the sparkles' colours. The numbers are the original's, as its own
// functions gave them, bit for bit.

#include <cstdint>

#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Magic/Gestures/GestureIcons.h"

using namespace openblack::magic::gestures;

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

LookingForArray Nothing()
{
	LookingForArray lf;
	lf.fill(LookingFor {0, -1, -1, -1});
	return lf;
}

/// Every shape as long as the circle, but the heart (13), twice as long
float Length(Gesture g)
{
	return g == 13 ? 2.0f : 1.0f;
}

const std::vector<glm::vec3>& Line(Gesture /*g*/)
{
	static const std::vector<glm::vec3> k_Line {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.5f}, {1.0f, 0.0f, 1.0f}};
	return k_Line;
}

const std::vector<glm::vec3>& NoPoints(Gesture /*g*/)
{
	static const std::vector<glm::vec3> k_None;
	return k_None;
}
} // namespace

TEST(GestureIcons, TheBoxesOfTheSlots)
{
	const auto first = IconBoxOf(0, {1024, 768});
	EXPECT_EQ(Bits(first.min.x), 0x44733333u);
	EXPECT_EQ(Bits(first.min.y), 0x44370A3Du);
	EXPECT_EQ(Bits(first.max.x), 0x447C28F6u);
	EXPECT_EQ(Bits(first.max.y), 0x44400000u);
	const auto third = IconBoxOf(2, {1024, 768});
	EXPECT_EQ(Bits(third.min.x), 0x445EB852u);
	EXPECT_EQ(Bits(third.max.x), 0x4467AE14u);
	const auto sixth = IconBoxOf(5, {1920, 1080});
	EXPECT_EQ(Bits(sixth.min.x), 0x44B40000u);
	EXPECT_EQ(Bits(sixth.min.y), 0x447D3333u);
	EXPECT_EQ(Bits(sixth.max.x), 0x44BC6667u);
	EXPECT_EQ(Bits(sixth.max.y), 0x44870000u);
	// the middle of the first box, cut to whole pixels
	EXPECT_EQ(IconPixel(first, 0.5f, 0.5f), glm::ivec2(990, 750));
}

TEST(GestureIcons, TheListIsMadeAgainOnlyWhenTheTableChanges)
{
	GestureIconList list;
	EXPECT_FALSE(list.Update(Nothing(), {1024, 768}, 0x00FF4646u, Length)) << "nothing looked for, as the list starts";
	auto lf = Nothing();
	lf[15] = {2, -1, 2, -1}; // the square spiral, the leash's start
	lf[1] = {8, -1, -1, -1}; // the spiral, the miracle selection
	lf[2] = {9, -1, -1, -1}; // the inverse spiral
	lf[13] = {6, -1, 3, -1}; // the heart, the good leash
	lf[7] = {4, -1, 1, -1};  // the vertical scribble, the evil leash
	lf[17] = {4, -1, 1, -1}; // the E, of the same type
	lf[5] = {0, 3, -1, -1};  // the scribble: no icon
	ASSERT_TRUE(list.Update(lf, {1024, 768}, 0x00FF4646u, Length));
	const auto icons = list.Icons();
	ASSERT_EQ(icons.size(), 6u);
	// the highest type first and, of the same type, the later gesture first
	const std::vector<Gesture> order {2, 1, 13, 17, 7, 15};
	for (size_t i = 0; i < icons.size(); ++i)
	{
		EXPECT_EQ(icons[i].gesture, order[i]) << i;
		const auto box = IconBoxOf(static_cast<int>(i), {1024, 768});
		EXPECT_EQ(icons[i].box.min, box.min);
		EXPECT_EQ(icons[i].symbolArgb, 0xFFFF4646u);
	}
	EXPECT_EQ(icons[2].leash, 3);
	EXPECT_FLOAT_EQ(icons[2].period, 4.0f) << "twice its shape's length against the circle's";
	EXPECT_FLOAT_EQ(icons[0].period, 2.0f);

	// the same table again keeps the list and its sparkles where they are
	list.Icons()[0].phase = 0.5f;
	EXPECT_FALSE(list.Update(lf, {1024, 768}, 0x00FF4646u, Length));
	EXPECT_FLOAT_EQ(list.Icons()[0].phase, 0.5f);
	// any change makes it again, from the start of the shapes
	lf[1].seed = 4;
	EXPECT_TRUE(list.Update(lf, {1024, 768}, 0x00FF4646u, Length));
	EXPECT_FLOAT_EQ(list.Icons()[0].phase, 0.0f);

	list.Clear();
	EXPECT_TRUE(list.Icons().empty());
	EXPECT_FALSE(list.Update(Nothing(), {1024, 768}, 0x00FF4646u, Length));
}

TEST(GestureIcons, TheyFadeOutWhileAHelpTextIsOnTheScreen)
{
	struct Case
	{
		float alpha;
		uint32_t ms;
		bool shown;
		uint32_t after;
		bool drawn;
	};
	const std::vector<Case> cases = {
	    {1.0f, 16, false, 0x3F800000u, true},   {1.0f, 16, true, 0x3F77CED9u, true},   {0.5f, 100, true, 0x3E99999Au, true},
	    {0.01f, 100, true, 0x00000000u, false}, {0.0f, 30, true, 0x00000000u, false},  {0.0f, 30, false, 0x3D75C290u, true},
	    {0.7f, 33, false, 0x3F441893u, true},   {0.99f, 33, false, 0x3F800000u, true}, {0.3f, 0, true, 0x3E99999Au, true},
	    {0.3f, 0, false, 0x3E99999Au, true},
	};
	for (const auto& c : cases)
	{
		float alpha = c.alpha;
		EXPECT_EQ(StepIconsAlpha(alpha, true, c.shown, c.ms), c.drawn) << c.alpha << " " << c.ms << " " << c.shown;
		EXPECT_EQ(Bits(alpha), c.after) << c.alpha << " " << c.ms << " " << c.shown;
	}
	// with no help system the alpha stands
	float alpha = 0.4f;
	EXPECT_TRUE(StepIconsAlpha(alpha, false, true, 50));
	EXPECT_EQ(Bits(alpha), 0x3ECCCCCDu);
}

TEST(GestureIcons, TheSparklesRunAlongTheShape)
{
	struct Case
	{
		float phase;
		float period;
		uint32_t ms;
		uint32_t phaseAfter;
		uint32_t secondsAfter;
		int count;
		uint32_t second;
	};
	const std::vector<Case> cases = {
	    {0.0f, 2.0f, 16, 0x3C03126Fu, 0x3C83126Fu, 1, 0},
	    // reaching 1 exactly goes round to 0
	    {0.95f, 2.0f, 100, 0x00000000u, 0x3DCCCCCDu, 1, 0},
	    // past 1 it goes back by 0.9
	    {0.99f, 1.5f, 33, 0x3DE56040u, 0x3D072B02u, 1, 0},
	    {0.2f, 2.6f, 50, 0x3E607E08u, 0x3D4CCCCDu, 1, 0},
	    // past 0.9, a second sparkle 0.9 behind
	    {0.92f, 2.0f, 16, 0x3F6D9169u, 0x3C83126Fu, 2, 0x3CE56060u},
	    {1.3f, 2.0f, 0, 0x3ECCCCCCu, 0x00000000u, 1, 0},
	};
	for (const auto& c : cases)
	{
		GestureIcon icon {.period = c.period, .phase = c.phase};
		const auto sparkles = StepIconSparkles(icon, c.ms);
		EXPECT_EQ(Bits(icon.phase), c.phaseAfter) << c.phase;
		EXPECT_EQ(Bits(icon.seconds), c.secondsAfter) << c.phase;
		ASSERT_EQ(sparkles.count, c.count) << c.phase;
		EXPECT_EQ(Bits(sparkles.at[0]), c.phaseAfter) << c.phase;
		if (c.count == 2)
		{
			EXPECT_EQ(Bits(sparkles.at[1]), c.second) << c.phase;
		}
	}
}

TEST(GestureIcons, TheColours)
{
	// the symbol: each byte of the player's colour scaled by the alpha
	EXPECT_EQ(IconSymbolArgb(0xFFFF4646u, 1.0f), 0xFEFE4545u);
	EXPECT_EQ(IconSymbolArgb(0xFF46FF46u, 0.5f), 0x7E227E22u);
	EXPECT_EQ(IconSymbolArgb(0xFF4646FFu, 0.3f), 0x4B14144Bu);
	EXPECT_EQ(IconSymbolArgb(0xFFFF4646u, 0.77f), 0xC3C33535u);
	// a sparkle: white, brightest halfway along, its alpha kept
	EXPECT_EQ(IconSparkleArgb(0.5f, 1.0f), 0x96FEFEFEu);
	EXPECT_EQ(IconSparkleArgb(0.25f, 1.0f), 0x96BEBEBEu);
	EXPECT_EQ(IconSparkleArgb(0.1f, 0.5f), 0x962D2D2Du);
	EXPECT_EQ(IconSparkleArgb(0.0f, 1.0f), 0x96000000u);
	EXPECT_EQ(IconSparkleArgb(0.9f, 0.8f), 0x96484848u);
	EXPECT_EQ(IconSparkleArgb(0.33f, 0.123f), 0x961B1B1Bu);
}

TEST(GestureIcons, AShapesPointAlongItsPoints)
{
	const auto& line = Line(0);
	EXPECT_EQ(ShapePointAt(line, -1.0f), line.front());
	EXPECT_EQ(ShapePointAt(line, 0.0f), line.front());
	EXPECT_EQ(ShapePointAt(line, 1.0f), line.back());
	EXPECT_EQ(ShapePointAt(line, 2.0f), line.back());
	// halfway through the points is the middle point; a quarter, halfway to it
	EXPECT_EQ(ShapePointAt(line, 0.5f), line[1]);
	EXPECT_EQ(ShapePointAt(line, 0.25f), glm::vec3(0.5f, 0.0f, 0.25f));
}

TEST(GestureIcons, AFrameDrawsTheSymbolThenItsSparkles)
{
	GestureIconList list;
	auto lf = Nothing();
	lf[17] = {4, -1, 1, -1}; // the E, on the second page of symbols
	ASSERT_TRUE(list.Update(lf, {1024, 768}, 0x00FF4646u, Length));
	float alpha = 1.0f;
	const auto draws = StepIcons(list.Icons(), alpha, true, false, 16, Line);
	ASSERT_EQ(draws.size(), 1u);
	EXPECT_EQ(draws[0].page, 1);
	EXPECT_EQ(draws[0].cell, 1);
	EXPECT_EQ(draws[0].symbolPixel, IconPixel(IconBoxOf(0, {1024, 768}), 0.5f, 0.5f));
	EXPECT_EQ(draws[0].symbolArgb, 0xFEFE4545u);
	EXPECT_EQ(draws[0].sparkles, 1);
	EXPECT_GT(list.Icons()[0].phase, 0.0f);

	// a shape with no points draws nothing of its icon, and its sparkles stand
	const float phase = list.Icons()[0].phase;
	EXPECT_TRUE(StepIcons(list.Icons(), alpha, true, false, 16, NoPoints).empty());
	EXPECT_EQ(list.Icons()[0].phase, phase);

	// the frame the fade reaches 0 draws nothing
	alpha = 0.01f;
	EXPECT_TRUE(StepIcons(list.Icons(), alpha, true, true, 100, Line).empty());
	EXPECT_EQ(list.Icons()[0].phase, phase);
}
