/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Help/Bubble.h"

// The did-you-know bubble's Bubble class.

using namespace openblack::help;

namespace
{
/// The bubble's dark colour
constexpr uint32_t k_Dark = 0xFF1D5E76;
/// Pixels from sums with the margin's third, which is not a whole number
constexpr float k_Pixel = 1e-3f;

/// A bubble 300 x 240 with its top left at (100, 100), a margin of 30 and lines 20 high: its text area runs from 110 to
/// 390 across, its bottom at 330, its fade in ends at 180 and its clip is at 332.5
BubbleLayout TestLayout()
{
	return {.width = 300.0f,
	        .height = 240.0f,
	        .depth = 50.0f,
	        .margin = 30.0f,
	        .textSize = 20.0f,
	        .fade = 1.0f,
	        .topLeft = {100.0f, 100.0f},
	        .tailPlace = 0.2f,
	        .tailRight = false};
}

/// Each part wrapped through the bubble's own caches, a character half the text size wide
BubbleWrapFn WrapOf(Bubble& bubble)
{
	return [&bubble](size_t part, std::u16string_view text, float maxWidth, float size) -> const std::vector<std::u16string>& {
		return bubble.PartWrap(part).Wrap(text, maxWidth, size,
		                                  [size](std::u16string_view t) { return 0.5f * size * static_cast<float>(t.size()); });
	};
}

/// A line's width: a character half the text size wide, as WrapOf measures it
float HalfSizeWide(std::u16string_view text, float size)
{
	return 0.5f * size * static_cast<float>(text.size());
}

std::optional<BubbleKeyBinding> NoKeys(int /*action*/)
{
	return std::nullopt;
}
} // namespace

TEST(DidYouKnowBubble, DefaultSizes)
{
	// the default sizes of (400, 250, 19)
	const auto s = DefaultBubbleSizes(400.0f, 250.0f, 19.0f);
	EXPECT_FLOAT_EQ(s.margin, 40.0f);
	EXPECT_FLOAT_EQ(s.farWidth, 200.0f / 7.0f);
	EXPECT_FLOAT_EQ(s.farHeight, 125.0f / 7.0f);
	EXPECT_FLOAT_EQ(s.farMargin, 200.0f / 35.0f); // half the width's 35th
	EXPECT_FLOAT_EQ(s.farTextSize, 19.0f / 7.0f);
}

TEST(DidYouKnowBubble, LayoutNearFarAndFade)
{
	const auto s = DefaultBubbleSizes(400.0f, 250.0f, 19.0f);
	EXPECT_FALSE(LayoutBubble(s, 0.5f, {400.0f, 300.0f}, 10.0f, 0.2f).has_value()); // depth < 1
	// at 3 m (the near depth): full size, fade 0 (it fades in up to 8 m)
	const auto near = LayoutBubble(s, 3.0f, {400.0f, 300.0f}, 10.0f, 0.2f);
	ASSERT_TRUE(near.has_value());
	EXPECT_FLOAT_EQ(near->width, 400.0f);
	EXPECT_FLOAT_EQ(near->fade, 0.0f);
	// top left = (sx + offset - place w, sy - margin - h)
	EXPECT_FLOAT_EQ(near->topLeft.x, 400.0f + 10.0f - 0.2f * 400.0f);
	EXPECT_FLOAT_EQ(near->topLeft.y, 300.0f - 40.0f - 250.0f);
	EXPECT_FALSE(near->tailRight);
	// at 100 m: fully visible
	EXPECT_FLOAT_EQ(LayoutBubble(s, 100.0f, {0.0f, 0.0f}, -10.0f, 0.5f)->fade, 1.0f);
	EXPECT_TRUE(LayoutBubble(s, 100.0f, {0.0f, 0.0f}, -10.0f, 0.5f)->tailRight);
	// at 450 m: far size, faded out
	const auto far = LayoutBubble(s, 450.0f, {0.0f, 0.0f}, 10.0f, 0.2f);
	// a + (b - a) x f in float, step by step as the original: a few ulp off b
	const float f = 1.0f;
	EXPECT_FLOAT_EQ(far->width, s.width + (s.farWidth - s.width) * f);
	EXPECT_FLOAT_EQ(far->fade, 0.0f);
}

TEST(DidYouKnowBubble, AnchorWhereverItFallsOnTheScreen)
{
	// on a screen 768 high, y turned to run from the top
	const auto inside = AnchorOnScreen({512.0f, 500.0f, 0.9f}, 20.0f, 3.5f, 768.0f);
	ASSERT_TRUE(inside.has_value());
	EXPECT_FLOAT_EQ(inside->x, 512.0f);
	EXPECT_FLOAT_EQ(inside->y, 268.0f);
	// past the left edge and above the top: still there, for the bubble that hangs from it
	const auto past = AnchorOnScreen({-12.5f, 900.0f, 0.9f}, 20.0f, 3.5f, 768.0f);
	ASSERT_TRUE(past.has_value());
	EXPECT_FLOAT_EQ(past->x, -12.5f);
	EXPECT_FLOAT_EQ(past->y, -132.0f);
	// at the near clip it is kept, nearer it is not
	EXPECT_TRUE(AnchorOnScreen({512.0f, 384.0f, 0.0f}, 3.5f, 3.5f, 768.0f).has_value());
	EXPECT_FALSE(AnchorOnScreen({512.0f, 384.0f, 0.0f}, 3.4f, 3.5f, 768.0f).has_value());
	EXPECT_FALSE(AnchorOnScreen({512.0f, 384.0f, 0.0f}, -10.0f, 3.5f, 768.0f).has_value());
	// and the layout takes a point past the edge as any other
	const auto layout = LayoutBubble(DefaultBubbleSizes(400.0f, 250.0f, 19.0f), 20.0f, *past, 10.0f, 0.8f);
	EXPECT_TRUE(layout.has_value());
}

TEST(DidYouKnowBubble, TailSide)
{
	EXPECT_FLOAT_EQ(TailFor(0.0f, 800).offset, 10.0f); // left edge: u = -1
	EXPECT_NEAR(TailFor(0.0f, 800).place, 0.8f, 1e-6f);
	EXPECT_FLOAT_EQ(TailFor(400.0f, 800).offset, 10.0f); // the middle: u = 0
	EXPECT_NEAR(TailFor(400.0f, 800).place, 0.2f, 1e-6f);
	EXPECT_FLOAT_EQ(TailFor(600.0f, 800).offset, -10.0f);  // right half
	EXPECT_FLOAT_EQ(TailFor(2000.0f, 800).offset, -10.0f); // past the right edge: u = 1
}

TEST(DidYouKnowBubble, DrawDepth)
{
	// level camera (pitch 0): 5 m in front of the sign; looking straight down: the near plane x 2
	EXPECT_FLOAT_EQ(DrawDepth(100.0f, 0.0f, 1.0f), 95.0f);
	EXPECT_FLOAT_EQ(DrawDepth(100.0f, 1.5707963705062866f, 1.0f), 2.0f);
	EXPECT_FLOAT_EQ(DrawDepth(4.0f, 0.0f, 1.0f), 2.0f); // never nearer than 2 x near
}

TEST(DidYouKnowBubble, LifeScrollAndDrag)
{
	Bubble b;
	b.KeepAlive();
	EXPECT_FLOAT_EQ(b.Life(), 3.0f);
	b.Age(5000.0f); // 5 s x 0.0002 per ms = 1.0
	EXPECT_FLOAT_EQ(b.Life(), 2.0f);
	b.Age(100000.0f);
	EXPECT_FLOAT_EQ(b.Life(), 0.0f);

	b.Restart(); // 1000 lines and no text yet: nothing to clamp to but 0
	auto marks = b.ClampScroll(250.0f, 40.0f, 19.0f);
	EXPECT_FLOAT_EQ(b.Scroll(), 1000.0f);
	b.SetTextHeight(300.0f);                     // the last draw's text: clamped to the last line that fits
	marks = b.ClampScroll(250.0f, 40.0f, 19.0f); // (300 - 170) / 19
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f);
	EXPECT_TRUE(marks.above);
	EXPECT_FALSE(marks.below);
	b.Drag(true, 100, 19.0f); // the first frame takes the point
	b.Drag(true, 81, 19.0f);  // 19 pixels up: one line back
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f - 1.0f);
	marks = b.ClampScroll(250.0f, 40.0f, 19.0f);
	EXPECT_FALSE(marks.below); // last - 1 > scroll is false at exactly one line back
	b.Drag(false, 0, 19.0f);
	b.Drag(true, 0, 19.0f); // a new drag starts from its own point
	EXPECT_FLOAT_EQ(b.Scroll(), 130.0f / 19.0f - 1.0f);
	b.Restart(); // the text height back to 0
	EXPECT_FLOAT_EQ(b.TextHeight(), 0.0f);
}

TEST(DidYouKnowBubble, ShapeCornersOnWholePixels)
{
	// a box from (100.25, 50.7) to (300.6, 200.9) with corners of 40.5 and a tail at 150.6, fading from 0 at the top to
	// the whole alpha at the bottom
	const auto shape = BubbleShape(100.25f, 50.7f, 300.6f, 200.9f, 40.5f, 150.6f, false, 0xFF1D5E76u, true, 0, 0xFF);
	ASSERT_EQ(shape.size(), 60u); // nine quads and the tail, two triangles each
	// the first corner quad: from (100, 50) to (140, 91), its corners cut to whole pixels
	EXPECT_FLOAT_EQ(shape[0].x, 100.0f);
	EXPECT_FLOAT_EQ(shape[0].y, 50.0f);
	EXPECT_FLOAT_EQ(shape[2].x, 140.0f);
	EXPECT_FLOAT_EQ(shape[2].y, 91.0f);
	EXPECT_FLOAT_EQ(shape[5].x, 100.0f);
	EXPECT_FLOAT_EQ(shape[5].y, 91.0f);
	// their alphas taken at the uncut heights: 0 at the top, 40.5 / 150.2 of 255 at 91.2, truncated
	EXPECT_EQ(shape[0].argb, 0x001D5E76u);
	EXPECT_EQ(shape[2].argb >> 24, 68u);
	// the far corner of the middle quad, at the uncut (260.1, 160.4)
	EXPECT_FLOAT_EQ(shape[26].x, 260.0f);
	EXPECT_FLOAT_EQ(shape[26].y, 160.0f);
	EXPECT_EQ(shape[26].argb >> 24, 186u);
	// the tail, kept 1.8 corners from the left: from (167.075, 197.2145) to (207.575, 237.7145)
	EXPECT_FLOAT_EQ(shape[54].x, 167.0f);
	EXPECT_FLOAT_EQ(shape[54].y, 197.0f);
	EXPECT_FLOAT_EQ(shape[56].x, 207.0f);
	EXPECT_FLOAT_EQ(shape[56].y, 237.0f);
}

TEST(DidYouKnowBubble, WrapGatheringText)
{
	// a fixed width of 10 per character
	const auto width = [](std::u16string_view s) { return 10.0f * static_cast<float>(s.size()); };
	auto lines = WrapGatheringText(u"one two three", 70.0f, width); // 7 characters a line
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"one two");
	EXPECT_EQ(lines[1], u"three");
	lines = WrapGatheringText(u"well-known", 60.0f, width); // breaks after the hyphen, kept
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"well-");
	EXPECT_EQ(lines[1], u"known");
	lines = WrapGatheringText(u"ab\ncd", 100.0f, width); // a line feed ends the line
	ASSERT_EQ(lines.size(), 2u);
	EXPECT_EQ(lines[0], u"ab");
	EXPECT_EQ(lines[1], u"cd");
}

TEST(DidYouKnowBubble, PartsAreTheTipABlankLineAndTheTitle)
{
	const auto parts = BubbleParts(u"one two three", u"Did you know?");
	EXPECT_EQ(parts[0], u"one two three");
	EXPECT_EQ(parts[1], u" ");
	EXPECT_EQ(parts[2], u"Did you know?");
}

TEST(DidYouKnowBubble, WordsAreLaidOutFromTheBottomUp)
{
	Bubble bubble;
	const auto parts = BubbleParts(u"one two three", u"Did you know?");
	const auto words = LayOutBubbleWords(TestLayout(), 0.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	// the tip on the area's bottom, a blank line, the title two lines above the tip
	EXPECT_FLOAT_EQ(words.textHeight, 60.0f);
	ASSERT_EQ(words.lines.size(), 6u);
	EXPECT_TRUE(words.keyIcons.empty());
	EXPECT_TRUE(words.gestures.empty());
	// each line centred across the area, 280 wide: the tip's 130 from 185. Its shadow first, in black at half the
	// alpha, its start and every bound two pixels down and right: its bottom edge at 332 is just where its own fade out
	// starts, and its clip runs from 102 to 334.5
	const auto& shadow = words.lines[0];
	EXPECT_EQ(shadow.run.font, TextFont::J0);
	EXPECT_EQ(shadow.run.text, u"one two three");
	EXPECT_NEAR(shadow.run.x, 187.0f, k_Pixel);
	EXPECT_FLOAT_EQ(shadow.run.y, 312.0f);
	EXPECT_FLOAT_EQ(shadow.run.size, 20.0f);
	EXPECT_EQ(shadow.run.r, 0);
	EXPECT_EQ(shadow.run.g, 0);
	EXPECT_EQ(shadow.run.b, 0);
	EXPECT_EQ(shadow.run.a, 128);
	EXPECT_EQ(shadow.alphaBottom, 128);
	EXPECT_FLOAT_EQ(shadow.run.clipTop, 102.0f);
	EXPECT_FLOAT_EQ(shadow.run.clipBottom, 334.5f);
	// then the line in white
	const auto& line = words.lines[1];
	EXPECT_EQ(line.run.text, u"one two three");
	EXPECT_NEAR(line.run.x, 185.0f, k_Pixel);
	EXPECT_FLOAT_EQ(line.run.y, 310.0f);
	EXPECT_EQ(line.run.r, 255);
	EXPECT_EQ(line.run.g, 255);
	EXPECT_EQ(line.run.b, 255);
	EXPECT_EQ(line.run.a, 255);
	EXPECT_EQ(line.alphaBottom, 255);
	EXPECT_FLOAT_EQ(line.run.clipTop, 100.0f);
	EXPECT_FLOAT_EQ(line.run.clipBottom, 332.5f);
	EXPECT_EQ(words.lines[3].run.text, u" ");
	EXPECT_FLOAT_EQ(words.lines[3].run.y, 290.0f);
	EXPECT_NEAR(words.lines[3].run.x, 245.0f, k_Pixel);
	EXPECT_EQ(words.lines[4].run.text, u"Did you know?");
	EXPECT_FLOAT_EQ(words.lines[4].run.y, 272.0f);
	EXPECT_EQ(words.lines[5].run.text, u"Did you know?");
	EXPECT_FLOAT_EQ(words.lines[5].run.y, 270.0f);
	EXPECT_NEAR(words.lines[5].run.x, 185.0f, k_Pixel);
}

TEST(DidYouKnowBubble, WordsFadeInUnderTheBoxTop)
{
	// a tip of twelve lines fills the box from 90 down: its first line starts above the box and fades in. Having
	// reached above the box's top, it is the last part laid out: the blank line and the title neither show nor count
	Bubble bubble;
	const auto parts = BubbleParts(u"a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl", u"Did you know?");
	const auto words = LayOutBubbleWords(TestLayout(), 0.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	EXPECT_FLOAT_EQ(words.textHeight, 240.0f);
	ASSERT_EQ(words.lines.size(), 24u);
	// the first line, from 90 to 110: its top above the box, its bottom (110 - 100) / 80 of the way in
	EXPECT_EQ(words.lines[1].run.text, u"a");
	EXPECT_FLOAT_EQ(words.lines[1].run.y, 90.0f);
	EXPECT_EQ(words.lines[1].run.a, 0);
	EXPECT_EQ(words.lines[1].alphaBottom, 31);
	// its shadow, from 92 to 112, of 128, fading in over its own bounds, from 102 to 182
	EXPECT_FLOAT_EQ(words.lines[0].run.y, 92.0f);
	EXPECT_EQ(words.lines[0].run.a, 0);
	EXPECT_EQ(words.lines[0].alphaBottom, 16);
	EXPECT_EQ(words.lines[23].run.text, u"l");
	EXPECT_FLOAT_EQ(words.lines[23].run.y, 310.0f);
}

TEST(DidYouKnowBubble, WordsPushedDownByTheScroll)
{
	// scrolled 30 pixels, the tip's one line starts at 340, under the clip at 332.5, and its shadow at 342, under its
	// own at 334.5: neither is drawn, though the tip's height counts
	Bubble bubble;
	const auto parts = BubbleParts(u"one two three", u"Did you know?");
	const auto words = LayOutBubbleWords(TestLayout(), 30.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	EXPECT_FLOAT_EQ(words.textHeight, 60.0f);
	ASSERT_EQ(words.lines.size(), 4u);
	// the blank line, from 320 to 340: its top at full alpha, its bottom past the clip
	EXPECT_EQ(words.lines[1].run.text, u" ");
	EXPECT_FLOAT_EQ(words.lines[1].run.y, 320.0f);
	EXPECT_EQ(words.lines[1].run.a, 255);
	EXPECT_EQ(words.lines[1].alphaBottom, 0);
	// the title, from 300 to 320, whole
	EXPECT_FLOAT_EQ(words.lines[3].run.y, 300.0f);
	EXPECT_EQ(words.lines[3].run.a, 255);
	EXPECT_EQ(words.lines[3].alphaBottom, 255);
}

TEST(DidYouKnowBubble, NoWordsWhereALineCannotFit)
{
	Bubble bubble;
	const auto parts = BubbleParts(u"ab", u"cd");
	// an area 9 across, less than half a line of 20: the words are wrapped and counted, one letter a line, but none is
	// drawn
	auto narrow = TestLayout();
	narrow.width = 29.0f;
	auto words = LayOutBubbleWords(narrow, 0.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	EXPECT_FLOAT_EQ(words.textHeight, 100.0f);
	EXPECT_TRUE(words.lines.empty());
	// a box 15 high: its clip runs from 100 to 107.5, less than a line; the tip, from 85, reaches above the box's top,
	// so it is the only part counted
	auto low = TestLayout();
	low.height = 15.0f;
	words = LayOutBubbleWords(low, 0.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	EXPECT_FLOAT_EQ(words.textHeight, 20.0f);
	EXPECT_TRUE(words.lines.empty());
}

TEST(DidYouKnowBubble, WordsAtHalfTheAlpha)
{
	Bubble bubble;
	const auto parts = BubbleParts(u"one two three", u"Did you know?");
	const auto words = LayOutBubbleWords(TestLayout(), 0.0f, 0.5f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	ASSERT_EQ(words.lines.size(), 6u);
	EXPECT_EQ(words.lines[0].run.a, 64);
	EXPECT_EQ(words.lines[1].run.a, 127);
}

TEST(DidYouKnowBubble, GestureSymbolAndKeyIconTakeTheirOwnSteps)
{
	// the tip's text up to its $g, then the symbol two lines high above it, then the blank line; the title is only a
	// $m icon, two lines high above that
	Bubble bubble;
	int asked = -1;
	const BubbleKeyFn keyOf = [&asked](int action) -> std::optional<BubbleKeyBinding> {
		asked = action;
		return BubbleKeyBinding {.animType = 1, .clickType = 2, .row = 3, .keyName = u"K"};
	};
	const auto parts = BubbleParts(u"Press$g05 later", u"$m12");
	const auto words = LayOutBubbleWords(TestLayout(), 0.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, keyOf);
	EXPECT_FLOAT_EQ(words.textHeight, 120.0f);
	ASSERT_EQ(words.lines.size(), 4u);
	EXPECT_EQ(words.lines[1].run.text, u"Press");
	EXPECT_NEAR(words.lines[1].run.x, 225.0f, k_Pixel);
	EXPECT_EQ(words.lines[3].run.text, u" ");
	EXPECT_FLOAT_EQ(words.lines[3].run.y, 250.0f);
	// the symbol 5: page 0, cell (1, 1), from 270 to 310; its shadow at +2 in the dark colour, then it in white
	ASSERT_EQ(words.gestures.size(), 2u);
	const auto& shadow = words.gestures[0];
	EXPECT_EQ(shadow.page, 0u);
	EXPECT_NEAR(shadow.x, 112.0f, k_Pixel);
	EXPECT_FLOAT_EQ(shadow.y, 272.0f);
	EXPECT_FLOAT_EQ(shadow.width, 40.0f);
	EXPECT_FLOAT_EQ(shadow.height, 40.0f);
	EXPECT_FLOAT_EQ(shadow.u, 0.25f);
	EXPECT_FLOAT_EQ(shadow.vTop, 0.25f);
	EXPECT_FLOAT_EQ(shadow.vBottom, 0.5f);
	EXPECT_EQ(shadow.abgr, 0xFF765E1Du);
	const auto& symbol = words.gestures[1];
	EXPECT_NEAR(symbol.x, 110.0f, k_Pixel);
	EXPECT_FLOAT_EQ(symbol.y, 270.0f);
	EXPECT_EQ(symbol.abgr, 0xFFFFFFFFu);
	// the icon of action 12, 40 across, centred on the area's middle, from 210 to 250
	EXPECT_EQ(asked, 12);
	ASSERT_EQ(words.keyIcons.size(), 1u);
	const auto& icon = words.keyIcons[0];
	EXPECT_EQ(icon.binding.animType, 1);
	EXPECT_EQ(icon.binding.clickType, 2);
	EXPECT_EQ(icon.binding.row, 3);
	EXPECT_EQ(icon.binding.keyName, u"K");
	EXPECT_NEAR(icon.x, 250, 1);
	EXPECT_EQ(icon.y, 230);
	EXPECT_EQ(icon.size, 40);
	EXPECT_EQ(icon.alpha, 255);
}

TEST(DidYouKnowBubble, GestureSymbolCutToTheBoxTop)
{
	// a symbol from 80 to 120 shows from the box top at 100 down, its picture cut by half its height's quarter
	Bubble bubble;
	const auto parts = BubbleParts(u"$g01", u"");
	const auto words = LayOutBubbleWords(TestLayout(), -210.0f, 1.0f, k_Dark, parts, WrapOf(bubble), HalfSizeWide, NoKeys);
	ASSERT_EQ(words.gestures.size(), 2u);
	// above the box's top, so the parts stop there
	EXPECT_FLOAT_EQ(words.textHeight, 40.0f);
	const auto& symbol = words.gestures[1];
	EXPECT_FLOAT_EQ(symbol.y, 100.0f);
	EXPECT_FLOAT_EQ(symbol.height, 20.0f);
	EXPECT_FLOAT_EQ(symbol.u, 0.25f);
	EXPECT_FLOAT_EQ(symbol.vTop, 0.125f);
	EXPECT_FLOAT_EQ(symbol.vBottom, 0.25f);
}

TEST(DidYouKnowBubble, PartWrapMatchesWrapGatheringText)
{
	// the bubble's three parts over a sequence of frames whose text, width and size change and come back: each cached
	// part breaks as WrapGatheringText does, and a part whose inputs did not change measures nothing
	int measures = 0;
	Bubble bubble;
	const std::array<std::u16string, Bubble::k_Parts> first = {u"Did you know that the well-known villagers pray?", u"",
	                                                           u"Tap to close"};
	const std::array<std::u16string, Bubble::k_Parts> second = {u"Another\nfact, shorter", u"", u"Tap to close"};
	for (int frame = 0; frame < 40; ++frame)
	{
		const auto& parts = frame < 10 || frame >= 30 ? first : second;
		const float maxWidth = frame >= 15 && frame < 20 ? 90.0f : 160.0f;
		const float size = frame >= 20 && frame < 25 ? 12.0f : 10.0f;
		for (size_t p = 0; p < Bubble::k_Parts; ++p)
		{
			const auto width = [size](std::u16string_view t) { return size * static_cast<float>(t.size()); };
			const auto counted = [&measures, &width](std::u16string_view t) {
				++measures;
				return width(t);
			};
			const auto expected = WrapGatheringText(parts.at(p), maxWidth, width);
			measures = 0;
			EXPECT_EQ(bubble.PartWrap(p).Wrap(parts.at(p), maxWidth, size, counted), expected) << frame << " " << p;
			const int afterFirst = measures;
			EXPECT_EQ(bubble.PartWrap(p).Wrap(parts.at(p), maxWidth, size, counted), expected) << frame << " " << p;
			EXPECT_EQ(measures, afterFirst);
			const bool changed = frame == 0 || frame == 10 || frame == 15 || frame == 20 || frame == 25 || frame == 30;
			if (!changed)
			{
				EXPECT_EQ(afterFirst, 0) << frame << " " << p;
			}
		}
	}
}
