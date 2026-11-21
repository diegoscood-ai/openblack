/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <vector>

#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLightbulb.h"

using namespace openblack;
using namespace openblack::creature_lightbulb;

TEST(CreatureLightbulb, AtMostOneInAHundredTurns)
{
	EXPECT_FALSE(MayMake(1000, 1100));
	EXPECT_TRUE(MayMake(1000, 1101));
	// the turns are unsigned, as the original's are
	EXPECT_TRUE(MayMake(0, 101));
}

TEST(CreatureLightbulb, ItIsMadeAboveTheHeadATenthOfTheHeightWithTheShareAsAPercent)
{
	const auto bulb = Make({10.0f, 20.0f, 30.0f}, 15.0f, 0.5f);
	EXPECT_FLOAT_EQ(bulb.position.y, 21.0f);
	EXPECT_FLOAT_EQ(bulb.size, 1.5f);
	EXPECT_EQ(bulb.text, u"50%");
	EXPECT_EQ(Text(1.0f), u"100%");
	EXPECT_EQ(Text(0.333f), u"33%");
}

TEST(CreatureLightbulb, ItGrowsRisesFadesAndBlinksForSixSeconds)
{
	auto bulb = Make({0.0f, 0.0f, 0.0f}, 10.0f, 1.0f);
	// at a second: grown by a sixth, risen 3 of its sizes a second, still opaque, bulb at a third of the glow's size
	auto look = Step(bulb, 1000);
	ASSERT_TRUE(look.has_value());
	EXPECT_FLOAT_EQ(look->glowSize, (1000.0f / 6000.0f + 1.0f) * 1.0f);
	EXPECT_FLOAT_EQ(bulb.position.y, 1.0f + 3.0f);
	EXPECT_EQ(look->alpha, 0xFF);
	EXPECT_FLOAT_EQ(look->bulbScale, look->glowSize / 3.0f);
	// a blink lasts a sixth of the life: at a second its glow is full again, and the glow's alpha half of the rest
	EXPECT_EQ(look->blink, 0xFF);
	EXPECT_EQ(look->glowAlpha, 0);
	// half way through the fade it is half as opaque
	look = Step(bulb, 3250);
	ASSERT_TRUE(look.has_value());
	EXPECT_EQ(look->alpha, 127);
	// at six seconds it is gone
	EXPECT_FALSE(Step(bulb, 1750).has_value());
}

TEST(CreatureLightbulb, ASightingShowsAFullBulbWhenLearntAndItsShareOnTheWay)
{
	// a normal action: a full bulb learnt now, half of one on the way, none known before or too young
	EXPECT_EQ(ForAction(true, false, false), std::optional(1.0f));
	EXPECT_EQ(ForAction(false, false, false), std::optional(0.5f));
	EXPECT_FALSE(ForAction(true, true, false).has_value());
	EXPECT_FALSE(ForAction(false, false, true).has_value());
	// a miracle: its share of the sightings on the way, a full bulb up to three sightings past its learning
	EXPECT_EQ(ForMiracle(false, 3.0f, 12.0f), std::optional(0.25f));
	EXPECT_EQ(ForMiracle(true, 12.0f, 12.0f), std::optional(1.0f));
	EXPECT_EQ(ForMiracle(true, 14.0f, 12.0f), std::optional(1.0f));
	EXPECT_FALSE(ForMiracle(true, 15.0f, 12.0f).has_value());
}

TEST(CreatureLightbulb, EachFrameAgesEveryBulbAndDropsThoseWhoseLifeIsOver)
{
	std::vector<Bulb> bulbs {Make({0.0f, 0.0f, 0.0f}, 10.0f, 1.0f), Make({5.0f, 0.0f, 0.0f}, 10.0f, 0.5f)};
	bulbs[0].ageMs = 5990;
	// a frame of 16 ms: the first one's life is over, the second is drawn, its look in the same order
	const auto looks = StepAll(bulbs, 16);
	ASSERT_EQ(bulbs.size(), 1u);
	ASSERT_EQ(looks.size(), 1u);
	EXPECT_EQ(bulbs[0].text, u"50%");
	EXPECT_EQ(bulbs[0].ageMs, 16u);
	EXPECT_FLOAT_EQ(looks[0].glow.x, 5.0f);
	// a paused frame ages nothing
	const auto paused = StepAll(bulbs, 0);
	ASSERT_EQ(paused.size(), 1u);
	EXPECT_EQ(bulbs[0].ageMs, 16u);
}

TEST(CreatureLightbulb, TheGlowIsYellowTheBulbWhiteWithItsBlinkAsItsBlue)
{
	const Look look {.blink = 0x40, .glowAlpha = 0x5F};
	EXPECT_EQ(GlowArgb(look), 0x5FFFFF00u);
	EXPECT_EQ(BulbRgb(look), 0xFFFF40u);
	EXPECT_EQ(BulbRgb(Look {.blink = 0xFF}), 0xFFFFFFu);
}

TEST(CreatureLightbulb, TheBulbStandsAtItsPointScaledAlikeAndNotTurned)
{
	const Look look {.bulb = {1.0f, 2.0f, 3.0f}, .bulbScale = 0.5f};
	const auto model = BulbModel(look);
	const auto corner = model * glm::vec4(2.0f, 4.0f, -2.0f, 1.0f);
	EXPECT_FLOAT_EQ(corner.x, 2.0f);
	EXPECT_FLOAT_EQ(corner.y, 4.0f);
	EXPECT_FLOAT_EQ(corner.z, 2.0f);
}

TEST(CreatureLightbulb, TheTextIsCentredOverItsPointAndRaised)
{
	// the middle of a 1024 x 768 screen, and a point off its left edge, still placed
	const auto middle = ScreenPoint({0.0f, 0.0f, 5.0f, 10.0f}, 1.0f, {1024.0f, 768.0f});
	ASSERT_TRUE(middle.has_value());
	EXPECT_FLOAT_EQ(middle->x, 512.0f);
	EXPECT_FLOAT_EQ(middle->y, 384.0f);
	const auto left = ScreenPoint({-20.0f, 10.0f, 5.0f, 10.0f}, 1.0f, {1024.0f, 768.0f});
	ASSERT_TRUE(left.has_value());
	EXPECT_FLOAT_EQ(left->x, -512.0f);
	EXPECT_FLOAT_EQ(left->y, 0.0f);
	// nearer the eye than the near plane: none
	EXPECT_FALSE(ScreenPoint({0.0f, 0.0f, 0.0f, 0.5f}, 1.0f, {1024.0f, 768.0f}).has_value());
	const auto at = TextPlace({512.0f, 384.0f}, 30.0f);
	EXPECT_FLOAT_EQ(at.x, 497.0f);
	EXPECT_FLOAT_EQ(at.y, 376.5f);
}
