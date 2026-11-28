/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include "Creature/CreatureStatusPanel.h"
#include "Graphics/Argb4444.h"

using namespace openblack;
using namespace openblack::creature_panel;

namespace
{
constexpr float k_Epsilon = 1e-4f;
const RewardTexts k_Texts {.bad = u"Bad Boy! %d%%", .good = u"Good Boy! %d%%", .none = u"No Reward"};

/// The layout with every point taken to the screen, as the panel draws it
Layout OnScreen(Layout layout)
{
	const auto onScreen = [&layout](Rect rect) {
		return Rect {.min = layout.ToScreen(rect.min), .max = layout.ToScreen(rect.max)};
	};
	layout.box = onScreen(layout.box);
	for (auto& row : layout.rows)
	{
		row.bar = onScreen(row.bar);
		row.labelRight = layout.ToScreen(row.labelRight);
		row.valueLeft = layout.ToScreen(row.valueLeft);
	}
	return layout;
}
} // namespace

TEST(CreatureStatusPanel, ItIsNotDrawnInASequenceNorUnderAnOpenBox)
{
	EXPECT_TRUE(Drawn(false, false));
	// inside the citadel or in the falling spell
	EXPECT_FALSE(Drawn(true, false));
	// under an open box, the tattoo editor's too
	EXPECT_FALSE(Drawn(false, true));
	EXPECT_FALSE(Drawn(true, true));
}

TEST(CreatureStatusPanel, ValuesComeFromTheBody)
{
	creature_physiology::Needs needs {};
	needs.life = 1.0f;
	needs.energy = 0.965f;
	needs.exhaustion = 0.32f;
	const auto values = FromNeeds(needs, 0.2f);
	EXPECT_FLOAT_EQ(values.damage, 0.0f);
	EXPECT_NEAR(values.hunger, 0.035f, k_Epsilon);
	EXPECT_FLOAT_EQ(values.tiredness, 0.32f);
	ASSERT_TRUE(values.reward.has_value());
	EXPECT_FLOAT_EQ(*values.reward, 0.2f);
	EXPECT_EQ(FormatPercent(values.damage), u"0%");
	EXPECT_EQ(FormatPercent(values.hunger), u"3%");
	EXPECT_EQ(FormatPercent(values.tiredness), u"32%");
	// Cut short in single precision, as the game is: 0.97 energy leaves 0.0299999 hunger, which shows as 2%
	needs.energy = 0.97f;
	EXPECT_EQ(FormatPercent(FromNeeds(needs, std::nullopt).hunger), u"2%");

	// A big meal fills it past full, which is no hunger; no reward without the hand
	needs.energy = 1.4f;
	needs.life = 0.25f;
	const auto fed = FromNeeds(needs, std::nullopt);
	EXPECT_FLOAT_EQ(fed.hunger, 0.0f);
	EXPECT_FLOAT_EQ(fed.damage, 0.75f);
	EXPECT_FALSE(fed.reward.has_value());
}

TEST(CreatureStatusPanel, ValuesAreClampedAsTheGameDoes)
{
	EXPECT_FLOAT_EQ(Clamp01(-0.5f), 0.0f);
	EXPECT_FLOAT_EQ(Clamp01(1.5f), 1.0f);
	EXPECT_FLOAT_EQ(Clamp01(0.5f), 0.5f);
	EXPECT_FLOAT_EQ(Clamp01(std::numeric_limits<float>::quiet_NaN()), 0.0f);
	EXPECT_FLOAT_EQ(ClampReward(-3.0f), -1.0f);
	EXPECT_FLOAT_EQ(ClampReward(3.0f), 1.0f);
	EXPECT_FLOAT_EQ(ClampReward(-0.4f), -0.4f);
	EXPECT_FLOAT_EQ(*FromNeeds({}, -2.0f).reward, -1.0f);
}

TEST(CreatureStatusPanel, PercentagesAreCutShort)
{
	EXPECT_EQ(Percent(0.0399f), 3);
	EXPECT_EQ(Percent(0.999f), 99);
	EXPECT_EQ(Percent(1.0f), 100);
	EXPECT_EQ(Percent(0.0f), 0);
	EXPECT_EQ(FormatNumber(u"%d%%", 42), u"42%");
	EXPECT_EQ(FormatNumber(u"Good Boy! %d%%", 7), u"Good Boy! 7%");
	// Anything else is left as it is
	EXPECT_EQ(FormatNumber(u"100% %s", 1), u"100% %s");
}

TEST(CreatureStatusPanel, RewardWords)
{
	EXPECT_EQ(Classify(0.005f), RewardKind::None);
	EXPECT_EQ(Classify(-0.01f), RewardKind::None);
	EXPECT_EQ(Classify(0.011f), RewardKind::Good);
	EXPECT_EQ(Classify(-0.011f), RewardKind::Bad);
	EXPECT_EQ(FormatReward(0.005f, k_Texts), u"No Reward 0%");
	EXPECT_EQ(FormatReward(-0.37f, k_Texts), u"Bad Boy! 37%");
	EXPECT_EQ(FormatReward(0.3f, k_Texts), u"Good Boy! 30%");
	EXPECT_EQ(FormatReward(1.0f, k_Texts), u"Good Boy! 100%");

	const Values values {.damage = 0.25f, .hunger = 0.5f, .tiredness = 0.0f, .reward = -0.5f};
	EXPECT_EQ(FormatValue(Row::Damage, values, k_Texts), u"25%");
	EXPECT_EQ(FormatValue(Row::Reward, values, k_Texts), u"Bad Boy! 50%");
}

TEST(CreatureStatusPanel, BarsFillAndColour)
{
	const Values values {.damage = 0.25f, .hunger = 0.5f, .tiredness = 1.0f, .reward = -0.5f};
	EXPECT_FLOAT_EQ(Fill(Row::Damage, values), 0.25f);
	EXPECT_FLOAT_EQ(Fill(Row::Tiredness, values), 1.0f);
	// The reward's bar is half full with none, empty slapped as hard as can be and full stroked
	EXPECT_FLOAT_EQ(Fill(Row::Reward, values), 0.25f);
	EXPECT_FLOAT_EQ(Fill(Row::Reward, Values {}), 0.5f);
	EXPECT_EQ(BarColour(Row::Hunger, 0.1f), glm::vec4(1.0f, 1.0f, 0.0f, 1.0f));
	EXPECT_EQ(BarColour(Row::Reward, 0.49f), glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
	EXPECT_EQ(BarColour(Row::Reward, 0.5f), glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
}

TEST(CreatureStatusPanel, BarFillRunsInsideTheFrame)
{
	const Rect bar {.min = {100.0f, 50.0f}, .max = {206.0f, 70.0f}};
	// Inside the three pixel frame: 100 pixels to fill
	const auto empty = FillOf(bar, 0.0f, false);
	EXPECT_TRUE(empty.Empty());
	EXPECT_FLOAT_EQ(empty.from, 103.0f);
	EXPECT_FLOAT_EQ(empty.top, 53.0f);
	EXPECT_FLOAT_EQ(empty.bottom, 67.0f);
	EXPECT_FALSE(empty.middle.has_value());
	EXPECT_FLOAT_EQ(FillOf(bar, 0.032f, false).to, 106.0f);
	EXPECT_FLOAT_EQ(FillOf(bar, 1.0f, false).to, 203.0f);
	EXPECT_FLOAT_EQ(FillOf(bar, 2.0f, false).to, 203.0f);

	// The reward fills from the middle either way, and none fills nothing
	const auto none = FillOf(bar, 0.5f, true);
	ASSERT_TRUE(none.middle.has_value());
	EXPECT_FLOAT_EQ(*none.middle, 153.0f);
	EXPECT_TRUE(none.Empty());
	const auto bad = FillOf(bar, 0.25f, true);
	EXPECT_FLOAT_EQ(bad.from, 153.0f);
	EXPECT_FLOAT_EQ(bad.to, 128.0f);
	EXPECT_FLOAT_EQ(FillOf(bar, 0.75f, true).to, 178.0f);
}

// The bars' frame is the front end atlas's box cell, black at 191 in its alpha file, as the game samples it from the
// atlas it keeps with four bits of alpha; the line round it is the bevel's white times the box's white
TEST(CreatureStatusPanel, TheBarsFrameIsTheAtlasBoxCutToFourBitsWithANearlyWhiteLine)
{
	EXPECT_EQ(k_BarFrameAlpha, 187);
	EXPECT_EQ(k_BarFrameAlpha, graphics::argb4444::Cut(191));
	EXPECT_EQ(k_BarLineGrey, 254);
}

TEST(CreatureStatusPanel, LayoutWithTheReward)
{
	// 1024 by 768: rows 48 apart, text 24 high
	const std::array<float, 4> labels {70.0f, 66.0f, 92.0f, 68.0f};
	const auto column = LabelColumnWidth(labels, 48.0f);
	EXPECT_FLOAT_EQ(column, 116.0f);
	const auto layout = OnScreen(Compute({1024, 768}, true, column));
	EXPECT_FLOAT_EQ(layout.size, 48.0f);
	EXPECT_FLOAT_EQ(layout.textSize, 24.0f);
	EXPECT_FLOAT_EQ(layout.box.min.x, 25.0f);
	EXPECT_FLOAT_EQ(layout.box.max.x, 281.0f);
	EXPECT_FLOAT_EQ(layout.box.min.y, 192.0f);
	EXPECT_FLOAT_EQ(layout.box.max.y, 448.0f);
	ASSERT_EQ(layout.Rows().size(), 4u);

	const auto& damage = layout.Rows()[0];
	EXPECT_EQ(damage.row, Row::Damage);
	EXPECT_FLOAT_EQ(damage.bar.min.x, 141.0f);
	EXPECT_FLOAT_EQ(damage.bar.max.x, 257.0f);
	EXPECT_FLOAT_EQ(damage.bar.min.y, 224.0f);
	EXPECT_FLOAT_EQ(damage.bar.max.y, 248.0f);
	EXPECT_FLOAT_EQ(damage.labelRight.x, 129.0f);
	EXPECT_FLOAT_EQ(damage.valueLeft.x, 269.0f);
	EXPECT_FLOAT_EQ(damage.labelRight.y, 224.0f);

	// A row apart, and a third of a row more before the reward
	EXPECT_FLOAT_EQ(layout.Rows()[2].bar.min.y, 320.0f);
	EXPECT_EQ(layout.Rows()[3].row, Row::Reward);
	EXPECT_FLOAT_EQ(layout.Rows()[3].bar.min.y, 384.0f);
}

TEST(CreatureStatusPanel, AtTheDefaultScreenEverythingIsInWholePixels)
{
	// 800 by 600: rows 37 apart, text 18 high, a gap of 9 by the bars
	EXPECT_EQ(RowPitch({800, 600}), 37);
	const std::array<float, 4> labels {60.0f, 58.0f, 80.0f, 61.0f};
	const auto column = LabelColumnWidth(labels, 37.0f);
	EXPECT_FLOAT_EQ(column, 98.0f);
	const auto layout = Compute({800, 600}, true, column);
	EXPECT_FLOAT_EQ(layout.textSize, 18.0f);
	EXPECT_EQ(layout.box.min, glm::vec2(20.0f, 150.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(220.0f, 350.0f));
	ASSERT_EQ(layout.Rows().size(), 4u);
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.min.y, 174.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[1].bar.min.y, 211.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[2].bar.min.y, 248.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[3].bar.min.y, 297.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.max.y, 192.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.min.x, 118.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.max.x, 216.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].labelRight.x, 109.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].valueLeft.x, 225.0f);

	// Following a creature: near the top, three quarters as high
	const auto follow = Compute({800, 600}, false, column);
	EXPECT_EQ(follow.box.min, glm::vec2(20.0f, 15.0f));
	EXPECT_EQ(follow.box.max, glm::vec2(220.0f, 165.0f));
	ASSERT_EQ(follow.Rows().size(), 3u);
	EXPECT_FLOAT_EQ(follow.Rows()[0].bar.min.y, 39.0f);
	EXPECT_FLOAT_EQ(follow.Rows()[2].bar.min.y, 113.0f);
}

TEST(CreatureStatusPanel, OnAWideScreenTheBoxTakesItsShareOfTheWholeWidth)
{
	// 1280 by 720: a fortieth of the width in from the left and a quarter of it wide, as on any other screen
	const auto layout = OnScreen(Compute({1280, 720}, true, 100.0f));
	EXPECT_FLOAT_EQ(layout.size, 45.0f);
	EXPECT_EQ(layout.box.min, glm::vec2(32.0f, 180.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(352.0f, 420.0f));
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.min.x, 132.0f);
	EXPECT_FLOAT_EQ(layout.Rows()[0].bar.max.x, 232.0f);
}

TEST(CreatureStatusPanel, AtTheDialogsSizeOrMoreItsSpaceIsCentredAPixelAUnit)
{
	EXPECT_EQ(RowPitch({1024, 768}), 48);
	EXPECT_EQ(DialogsToScreen({800, 600}, {-7.0f, 35.0f}), glm::vec2(-7.0f, 35.0f));
	EXPECT_EQ(DialogsToScreen({1024, 768}, {29.0f, 140.0f}), glm::vec2(141.0f, 224.0f));
	EXPECT_EQ(DialogsToScreen({1280, 720}, {-208.0f, 120.0f}), glm::vec2(32.0f, 180.0f));
	const auto layout = Compute({1024, 768}, true, 116.0f);
	EXPECT_FLOAT_EQ(layout.TextSizeOnScreen(), 24.0f);
	EXPECT_EQ(layout.box.min, glm::vec2(-87.0f, 108.0f));
}

TEST(CreatureStatusPanel, BelowTheDialogsSizeItsSpaceShrinksAndEachPointIsCutToAWholePixel)
{
	// 640 by 480 shrinks the space by 650 / 480 (850 / 640 is smaller), centred, each axis cut towards zero; the
	// points are the game's own, from both ways of its mapping
	EXPECT_EQ(DialogsToScreen({640, 480}, {-10.0f, -8.0f}), glm::vec2(17.0f, 12.0f));
	EXPECT_EQ(DialogsToScreen({640, 480}, {150.0f, 38.0f}), glm::vec2(135.0f, 46.0f));
	EXPECT_EQ(DialogsToScreen({640, 480}, {204.0f, 153.0f}), glm::vec2(175.0f, 131.0f));
	EXPECT_EQ(DialogsToScreen({640, 480}, {-11.0f, 138.0f}), glm::vec2(16.0f, 120.0f));
	EXPECT_EQ(DialogsToScreen({800, 480}, {-13.0f, 162.0f}), glm::vec2(95.0f, 138.0f));
	// A row is a sixteenth of the screen's height in pixels, 30, taken into the space: 40 units
	EXPECT_EQ(RowPitch({640, 480}), 40);
}

TEST(CreatureStatusPanel, LayoutFollowingACreature)
{
	// 640 by 480, smaller than the dialogs' space: the box's corners at the same shares of the screen, near the top
	// and without the reward, are taken into the space and laid out there, so they come back a pixel off
	const auto layout = Compute({640, 480}, false, 80.0f);
	EXPECT_FLOAT_EQ(layout.size, 40.0f);
	EXPECT_FLOAT_EQ(layout.textSize, 20.0f);
	EXPECT_NEAR(layout.TextSizeOnScreen(), 20.0f * 480.0f / 650.0f, k_Epsilon);
	EXPECT_EQ(layout.box.min, glm::vec2(-10.0f, -8.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(204.0f, 153.0f));
	ASSERT_EQ(layout.Rows().size(), 3u);
	EXPECT_EQ(layout.Rows()[0].bar.min, glm::vec2(70.0f, 18.0f));
	EXPECT_EQ(layout.Rows()[2].bar.max, glm::vec2(150.0f, 118.0f));

	const auto screen = OnScreen(layout);
	EXPECT_EQ(screen.box.min, glm::vec2(17.0f, 12.0f));
	EXPECT_EQ(screen.box.max, glm::vec2(175.0f, 131.0f));
	EXPECT_EQ(screen.Rows()[0].bar.min, glm::vec2(76.0f, 31.0f));
	EXPECT_EQ(screen.Rows()[0].bar.max, glm::vec2(135.0f, 46.0f));
	EXPECT_EQ(screen.Rows()[0].labelRight, glm::vec2(68.0f, 31.0f));
	EXPECT_EQ(screen.Rows()[0].valueLeft, glm::vec2(142.0f, 31.0f));
	EXPECT_EQ(screen.Rows()[1].bar.min, glm::vec2(76.0f, 61.0f));
	EXPECT_EQ(screen.Rows()[2].bar.max, glm::vec2(135.0f, 105.0f));
	// The texts' shadows, two units down and right, land two pixels or one away
	EXPECT_EQ(layout.ToScreen(layout.Rows()[0].labelRight + 2.0f), glm::vec2(70.0f, 33.0f));
	EXPECT_EQ(layout.ToScreen(layout.Rows()[1].labelRight + 2.0f), glm::vec2(70.0f, 62.0f));
}

TEST(CreatureStatusPanel, LayoutWithTheRewardOnASmallScreen)
{
	// 640 by 480 with the reward, and 800 by 480, too short alone: the space shrinks by the same share on both
	const auto layout = OnScreen(Compute({640, 480}, true, 100.0f));
	EXPECT_EQ(layout.box.min, glm::vec2(17.0f, 118.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(175.0f, 279.0f));
	ASSERT_EQ(layout.Rows().size(), 4u);
	EXPECT_EQ(layout.Rows()[0].bar.min, glm::vec2(91.0f, 138.0f));
	EXPECT_EQ(layout.Rows()[0].bar.max, glm::vec2(164.0f, 152.0f));
	EXPECT_EQ(layout.Rows()[2].bar.min, glm::vec2(91.0f, 197.0f));
	EXPECT_EQ(layout.Rows()[3].bar.min, glm::vec2(91.0f, 236.0f));
	EXPECT_EQ(layout.Rows()[3].bar.max, glm::vec2(164.0f, 251.0f));
	EXPECT_EQ(layout.Rows()[3].labelRight, glm::vec2(83.0f, 236.0f));
	EXPECT_EQ(layout.Rows()[3].valueLeft, glm::vec2(172.0f, 236.0f));

	const auto wide = Compute({800, 480}, true, 100.0f);
	EXPECT_EQ(wide.box.min, glm::vec2(-113.0f, 136.0f));
	EXPECT_EQ(wide.box.max, glm::vec2(155.0f, 353.0f));
	const auto wideOnScreen = OnScreen(wide);
	EXPECT_EQ(wideOnScreen.box.min, glm::vec2(21.0f, 118.0f));
	EXPECT_EQ(wideOnScreen.box.max, glm::vec2(219.0f, 279.0f));
	EXPECT_EQ(wideOnScreen.Rows()[0].bar.min, glm::vec2(95.0f, 138.0f));
	EXPECT_EQ(wideOnScreen.Rows()[0].bar.max, glm::vec2(168.0f, 152.0f));
}

TEST(CreatureStatusPanel, ShowsOnHover)
{
	EXPECT_EQ(k_Trigger, Trigger::Hover);
	EXPECT_TRUE(Triggered(false));
	EXPECT_TRUE(Triggered(true));
}
