/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// help::script_spirits (src/Help/ScriptSpirits.h): which advisor a script's spirit means, who says a help text and the
// script's screen fractions and the pixel they name.

#include <cstdint>

#include <bit>
#include <functional>
#include <limits>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Help/ScriptSpirits.h"

using namespace openblack::help::script_spirits;

namespace
{
/// A random draw the test can count
struct FakeRand
{
	uint32_t value {0};
	int calls {0};

	[[nodiscard]] std::function<uint32_t()> Fn()
	{
		return [this]() {
			++calls;
			return value;
		};
	}
};
} // namespace

TEST(ScriptSpirits, DiscreteAlignmentHasSevenSteps)
{
	EXPECT_EQ(DiscreteAlignment(-1.0f), 0);
	EXPECT_EQ(DiscreteAlignment(0.0f), 3);
	// Only the most good alignment reaches the last step
	EXPECT_EQ(DiscreteAlignment(1.0f), 6);
	EXPECT_EQ(DiscreteAlignment(0.99f), 6);
	EXPECT_EQ(DiscreteAlignment(0.7f), 5);
	EXPECT_EQ(DiscreteAlignment(-0.72f), 0);
	EXPECT_EQ(DiscreteAlignment(-0.71f), 1);
	// Above the range it is held at the most good
	EXPECT_EQ(DiscreteAlignment(5.0f), 6);
}

TEST(ScriptSpirits, DiscreteAlignmentMatchesTheOriginalAtTheStepsEdges)
{
	// The original's answers (the exe oracle, on the game's SSE2 conversion): it multiplies by the float just below 7,
	// so these two, a hair under the edges of steps 3 and 6, stay on the step below; a NaN passes the cap and, with
	// minus infinity and a value far below the range, converts to the integer indefinite
	constexpr int32_t k_Indefinite = std::numeric_limits<int32_t>::min();
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0xBE124923U)), 2);
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0x3F36DB6EU)), 5);
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0x7F800000U)), 6);
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0x7FC00000U)), k_Indefinite);
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0xFF800000U)), k_Indefinite);
	EXPECT_EQ(DiscreteAlignment(std::bit_cast<float>(0xD0000000U)), k_Indefinite);
}

TEST(ScriptSpirits, FixedSpiritsDrawNoRandomNumber)
{
	FakeRand rand;
	EXPECT_EQ(HelpSpiritOf(0, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(1, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(2, 3, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(99, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 0);
}

TEST(ScriptSpirits, AlignmentSpiritsGoByThePlayer)
{
	FakeRand rand;
	// The player's alignment: evil below neutral
	EXPECT_EQ(HelpSpiritOf(3, 2, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(3, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(3, 6, rand.Fn()), k_GoodSpirit);
	// Its opposite: evil from neutral up
	EXPECT_EQ(HelpSpiritOf(4, 2, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(HelpSpiritOf(4, 3, rand.Fn()), k_EvilSpirit);
	EXPECT_EQ(HelpSpiritOf(4, 0, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 0);
}

TEST(ScriptSpirits, RandomSpiritIsEvilAboveHalf)
{
	FakeRand rand;
	rand.value = 50;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_GoodSpirit);
	rand.value = 51;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_EvilSpirit);
	rand.value = 0;
	EXPECT_EQ(HelpSpiritOf(5, 3, rand.Fn()), k_GoodSpirit);
	EXPECT_EQ(rand.calls, 3);
	// No random numbers: the good one
	EXPECT_EQ(HelpSpiritOf(5, 3, {}), k_GoodSpirit);
}

TEST(ScriptSpirits, AdvisorNarratorsTalk)
{
	EXPECT_EQ(SpiritWhoTalks(2), k_GoodSpirit);
	EXPECT_EQ(SpiritWhoTalks(3), k_EvilSpirit);
	EXPECT_EQ(SpiritWhoTalks(0), 0);
	EXPECT_EQ(SpiritWhoTalks(1), 0);
	EXPECT_EQ(SpiritWhoTalks(4), 0);
}

TEST(ScriptSpirits, ScreenFractionsAreZeroToOne)
{
	EXPECT_TRUE(IsScreenFraction(0.0f));
	EXPECT_TRUE(IsScreenFraction(0.75f));
	EXPECT_TRUE(IsScreenFraction(1.0f));
	EXPECT_FALSE(IsScreenFraction(-0.01f));
	EXPECT_FALSE(IsScreenFraction(1.01f));
	EXPECT_FALSE(IsScreenFraction(std::numeric_limits<float>::quiet_NaN()));
}

// SPIRIT_SCREEN_POINT's pixel: the screen's width times the fraction across and its height times the fraction down,
// each truncated
TEST(ScriptSpirits, ScreenFractionsNameThePixelTruncated)
{
	EXPECT_EQ(ScreenPixel(640, 480, 0.5f, 0.25f), glm::ivec2(320, 120));
	EXPECT_EQ(ScreenPixel(640, 480, 0.0f, 1.0f), glm::ivec2(0, 480));
	// a third of 640 is 213.33: 213; 0.999 of 480 is 479.52: 479
	EXPECT_EQ(ScreenPixel(640, 480, 1.0f / 3.0f, 0.999f), glm::ivec2(213, 479));
	EXPECT_EQ(ScreenPixel(1024, 768, 0.75f, 0.1f), glm::ivec2(768, 76));
	// the fraction is not checked here: the native only reports one out of range
	EXPECT_EQ(ScreenPixel(640, 480, 1.5f, -0.25f), glm::ivec2(960, -120));
}

// SPIRIT_POINT_POS and SPIRIT_POINT_GAME_THING: the in-world value is compared with 1 as it is popped, so only 1 points
// in the world (the scripts push 0 or 1)
TEST(ScriptSpirits, OnlyOnePointsInTheWorld)
{
	EXPECT_TRUE(PointsInTheWorld(1u));
	EXPECT_FALSE(PointsInTheWorld(0u));
	EXPECT_FALSE(PointsInTheWorld(2u));
	EXPECT_FALSE(PointsInTheWorld(0xFFFFFFFFu));
	// a float 1 pushed in its place is its bits, not 1
	EXPECT_FALSE(PointsInTheWorld(std::bit_cast<uint32_t>(1.0f)));
}
