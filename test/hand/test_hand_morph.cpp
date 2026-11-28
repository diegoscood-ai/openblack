/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "3D/HandMorph.h"
#include "Creature/CreatureSkin.h"

using namespace openblack;
using hand_morph::Look;

TEST(HandMorph, TargetIsTheAlignmentHeldToItsRange)
{
	EXPECT_FLOAT_EQ(hand_morph::Target(0.4f), 0.4f);
	EXPECT_FLOAT_EQ(hand_morph::Target(-0.4f), -0.4f);
	EXPECT_FLOAT_EQ(hand_morph::Target(-3.0f), -1.0f);
	EXPECT_FLOAT_EQ(hand_morph::Target(3.0f), 1.0f);
	EXPECT_FLOAT_EQ(hand_morph::Target(-1.0f), -1.0f);
	EXPECT_FLOAT_EQ(hand_morph::Target(1.0f), 1.0f);
	// Changed from his -1: ours clamps with std::clamp, which lets what isn't a number through as it is
	EXPECT_TRUE(std::isnan(hand_morph::Target(std::numeric_limits<float>::quiet_NaN())));
}

TEST(HandMorph, StaysUntilTheAlignmentMovesFarEnough)
{
	EXPECT_FALSE(hand_morph::Refresh(0.0f, 0.0f).has_value());
	EXPECT_FALSE(hand_morph::Refresh(0.0f, 0.029f).has_value());
	EXPECT_FALSE(hand_morph::Refresh(0.5f, 0.48f).has_value());
	EXPECT_FALSE(hand_morph::Refresh(0.0f, std::numeric_limits<float>::quiet_NaN()).has_value());
}

TEST(HandMorph, JumpsStraightToTheAlignmentOnceFarEnough)
{
	ASSERT_TRUE(hand_morph::Refresh(0.0f, 0.03f).has_value());
	EXPECT_FLOAT_EQ(*hand_morph::Refresh(0.0f, 0.03f), 0.03f);
	ASSERT_TRUE(hand_morph::Refresh(0.0f, -1.0f).has_value());
	EXPECT_FLOAT_EQ(*hand_morph::Refresh(0.0f, -1.0f), -1.0f);
	// Back to neutral at once
	ASSERT_TRUE(hand_morph::Refresh(-0.5f, 0.0f).has_value());
	EXPECT_FLOAT_EQ(*hand_morph::Refresh(-0.5f, 0.0f), 0.0f);
}

TEST(HandMorph, SlowChangesComeInSteps)
{
	float drawn = 0.0f;
	int steps = 0;
	for (int i = 1; i <= 100; ++i)
	{
		if (const auto next = hand_morph::Refresh(drawn, static_cast<float>(i) * 0.01f))
		{
			drawn = *next;
			++steps;
		}
	}
	// Every third hundredth, give or take rounding
	EXPECT_GE(steps, 25);
	EXPECT_LE(steps, 34);
	EXPECT_GT(drawn, 0.96f);
}

TEST(HandMorph, PullsTowardsEvilBelowZeroAndGoodFromZero)
{
	EXPECT_EQ(hand_morph::LookOf(-0.01f), Look::Evil);
	EXPECT_EQ(hand_morph::LookOf(0.0f), Look::Good);
	EXPECT_EQ(hand_morph::LookOf(0.7f), Look::Good);
	EXPECT_FLOAT_EQ(hand_morph::Weight(-0.25f), 0.25f);
	EXPECT_FLOAT_EQ(hand_morph::Weight(0.6f), 0.6f);
}

TEST(HandMorph, SkinBlendsEveryChannelInWholeSteps)
{
	const std::array<uint16_t, 3> base {0x0000, 0xFFFF, 0x1234};
	const std::array<uint16_t, 3> look {0xFFFF, 0x0000, 0x4321};
	std::array<uint16_t, 3> blended {};

	// Neutral is the base, the far end the other skin
	hand_morph::BlendSkin(base, look, 0.0f, blended);
	EXPECT_EQ(blended, base);
	hand_morph::BlendSkin(base, look, -1.0f, blended);
	EXPECT_EQ(blended, look);

	// Halfway: 128 steps of 255, each 4-bit channel rounded down, alpha too
	hand_morph::BlendSkin(base, look, 0.5f, blended);
	EXPECT_EQ(blended[0], 0x7777); // 15 * 128 / 255 = 7.53
	EXPECT_EQ(blended[1], 0x7777); // 15 * 127 / 255 = 7.47
	// Each channel's two sum to 5: from 2.49 to 2.51
	EXPECT_EQ(blended[2], 0x2222);
}

TEST(HandMorph, SkinWeightIsTruncated)
{
	const std::array<uint16_t, 1> base {0x0000};
	const std::array<uint16_t, 1> look {0x000F};
	std::array<uint16_t, 1> blended {};
	// 0.0039 * 256 is under one step: nothing moves
	hand_morph::BlendSkin(base, look, 0.0039f, blended);
	EXPECT_EQ(blended[0], 0x0000);
	// 17 steps of 255 is the first whole step of a 4-bit channel
	hand_morph::BlendSkin(base, look, 17.0f / 256.0f, blended);
	EXPECT_EQ(blended[0], 0x0001);
	hand_morph::BlendSkin(base, look, 16.9f / 256.0f, blended);
	EXPECT_EQ(blended[0], 0x0000);
}

TEST(HandMorph, SkinBlendIsTheCreatureSkinsBlend)
{
	// The hand's own blend before it took a creature's: each 4-bit channel blended in place and masked back, and the
	// weight |drawn| in 256 steps, truncated, at most 255
	const auto maskedTexel = [](uint16_t from, uint16_t to, uint32_t weight) {
		uint32_t out = 0;
		for (const uint32_t mask : {0xFu, 0xF0u, 0xF00u, 0xF000u})
		{
			out |= (((from & mask) * (255u - weight) + (to & mask) * weight) / 255u) & mask;
		}
		return static_cast<uint16_t>(out);
	};
	const auto truncatedWeight = [](float drawn) {
		return std::min<uint32_t>(static_cast<uint32_t>(std::abs(drawn) * 256.0f), 255u);
	};

	// Every texel, at every weight, against a scrambled partner (an odd multiplier is a bijection on 16 bits)
	uint32_t texelMismatches = 0;
	for (uint32_t weight = 0; weight <= creature_skin::k_MaxWeight; ++weight)
	{
		for (uint32_t i = 0; i <= 0xFFFFu; ++i)
		{
			const auto from = static_cast<uint16_t>(i);
			const auto to = static_cast<uint16_t>(i * 40503u);
			if (creature_skin::BlendTexel(from, to, static_cast<uint8_t>(weight)) != maskedTexel(from, to, weight))
			{
				++texelMismatches;
			}
		}
	}
	EXPECT_EQ(texelMismatches, 0u);

	// Every alignment from -1 to 1 in steps of 1/4096, the exact steps of 1/256, and past the ends
	for (int32_t i = -4096 - 64; i <= 4096 + 64; ++i)
	{
		const float drawn = static_cast<float>(i) / 4096.0f;
		ASSERT_EQ(creature_skin::BlendWeight(drawn), truncatedWeight(drawn)) << "drawn " << drawn;
	}
	for (const float drawn : {-3.0f, 3.0f, 255.0f / 256.0f, -255.0f / 256.0f, std::nextafter(1.0f, 0.0f)})
	{
		EXPECT_EQ(creature_skin::BlendWeight(drawn), truncatedWeight(drawn)) << "drawn " << drawn;
	}
}

TEST(HandMorph, AFrameTakesTheAlignmentAndCatchesUpOnceFarEnough)
{
	hand_morph::State state;
	auto change = hand_morph::Advance(state, 0.02f, std::nullopt);
	EXPECT_FALSE(change.skin.has_value());
	EXPECT_FALSE(change.shape);
	EXPECT_FLOAT_EQ(state.target, 0.02f);
	EXPECT_FLOAT_EQ(state.drawn, 0.0f);

	change = hand_morph::Advance(state, -2.0f, std::nullopt);
	ASSERT_TRUE(change.skin.has_value());
	EXPECT_FLOAT_EQ(*change.skin, -1.0f);
	EXPECT_TRUE(change.shape);
	EXPECT_FLOAT_EQ(state.drawn, -1.0f);
}

TEST(HandMorph, CrossingTheInfluenceBlendsTheSkinForTheLastAlignmentTaken)
{
	hand_morph::State state;
	// It starts as if in the influence: being in it changes nothing
	EXPECT_FALSE(hand_morph::Advance(state, 0.0f, true).skin.has_value());
	EXPECT_FALSE(hand_morph::Advance(state, 0.02f, std::nullopt).skin.has_value());

	// Leaving it blends the skin for the alignment of the frame before, not this frame's, and leaves the shape
	auto change = hand_morph::Advance(state, 0.025f, false);
	ASSERT_TRUE(change.skin.has_value());
	EXPECT_FLOAT_EQ(*change.skin, 0.02f);
	EXPECT_FALSE(change.shape);
	EXPECT_FLOAT_EQ(state.drawn, 0.0f);
	EXPECT_FALSE(state.inInfluence);

	// Not knowing where the cursor is keeps it as it was
	EXPECT_FALSE(hand_morph::Advance(state, 0.025f, std::nullopt).skin.has_value());
	EXPECT_FALSE(state.inInfluence);

	// Coming back while the alignment moves far enough: the frame's alignment wins
	change = hand_morph::Advance(state, 0.5f, true);
	ASSERT_TRUE(change.skin.has_value());
	EXPECT_FLOAT_EQ(*change.skin, 0.5f);
	EXPECT_TRUE(change.shape);
}

TEST(HandMorph, TestsThePickOrElseTheOriginUnlessHeld)
{
	const auto last = map_coords::FromMetres({10.0f, 20.0f});
	const auto picked = map_coords::FromMetres({30.0f, 40.0f});
	EXPECT_EQ(hand_morph::NextPoint(last, picked, false), picked);
	EXPECT_EQ(hand_morph::NextPoint(last, picked, true), picked);
	EXPECT_EQ(hand_morph::NextPoint(last, std::nullopt, false), map_coords::MapCoords {});
	EXPECT_EQ(hand_morph::NextPoint(last, std::nullopt, true), last);
}
