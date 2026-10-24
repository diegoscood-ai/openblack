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

#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureAttention.h"

using namespace openblack;
using namespace openblack::creature_attention;

namespace
{
/// A creature `distance` from a camera at the origin looking along +x, `degrees` off the way it looks
glm::vec3 Around(float distance, float degrees)
{
	const auto radians = degrees * 3.14159265f / 180.0f;
	return {distance * std::cos(radians), 0.0f, distance * std::sin(radians)};
}
constexpr glm::vec3 k_Origin {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 k_Ahead {10.0f, 0.0f, 0.0f};
} // namespace

TEST(CreatureAttention, TheCameraLooksAtACreatureNearAndAheadOfIt)
{
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 0.0f)));
	// Within two thirds of seventy degrees either way
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 46.0f)));
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, -46.0f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 47.5f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(100.0f, 180.0f)));
}

TEST(CreatureAttention, NotACreatureTooFarAway)
{
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, Around(399.0f, 0.0f)));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, Around(400.0f, 0.0f)));
}

TEST(CreatureAttention, NotACreatureAtTheCameraNorFromACameraLookingNowhere)
{
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, k_Origin));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Origin, Around(100.0f, 0.0f)));
}

TEST(CreatureAttention, TheLookingAngleIsTheGamesTwoThirdsOfSeventyDegrees)
{
	// the game's field of view and two thirds, as floats, multiplied at a float's precision
	EXPECT_EQ(std::bit_cast<uint32_t>(1.2217305f), 0x3F9C61AAu);
	EXPECT_EQ(std::bit_cast<uint32_t>(0.6666667f), 0x3F2AAAABu);
	EXPECT_EQ(k_LookingAngle, 1.2217305f * 0.6666667f);
	// its cosine as the FPU leaves it lies just above the nearest float, so the float is not inside the angle
	const double cosine = std::cos(static_cast<double>(k_LookingAngle));
	EXPECT_NEAR(cosine, 0.68624164065, 1e-11);
	EXPECT_LT(static_cast<double>(static_cast<float>(cosine)), cosine);
}

TEST(CreatureAttention, ACreatureRightOnTheAnglesEdgeIsNotLookedAt)
{
	// one metre away, its way from the camera ahead by exactly the cosine as a float
	const glm::vec3 ahead {1.0f, 0.0f, 0.0f};
	const float edge = 0x1.5f5b10p-1f;
	EXPECT_EQ(edge, static_cast<float>(std::cos(static_cast<double>(k_LookingAngle))));
	EXPECT_FALSE(IsLookingAt(k_Origin, ahead, {edge, 0x1.746a52p-1f, 0.0f}));
	EXPECT_TRUE(IsLookingAt(k_Origin, ahead, {std::nextafter(edge, 1.0f), 0x1.746a52p-1f, 0.0f}));
}

TEST(CreatureAttention, TheLookIsSummedZFirstAndMadeAUnitByItsReciprocal)
{
	// two creatures right by the angle's edge where the order of the sums and dividing by the distance (rather than
	// multiplying by its reciprocal) would give the other answer
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, {0x1.bfddaap+4f, 0x1.6f6514p+3f, 0x1.b5ba1cp+4f}));
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, {0x1.6b5078p+6f, 0x1.0b4140p+6f, 0x1.1540fap+6f}));
}

TEST(CreatureAttention, TheRangeIsCheckedOnTheDistanceAsAFloat)
{
	EXPECT_TRUE(IsLookingAt(k_Origin, k_Ahead, {std::nextafter(400.0f, 0.0f), 0.0f, 0.0f}));
	EXPECT_FALSE(IsLookingAt(k_Origin, k_Ahead, {400.0f, 0.0f, 0.0f}));
}

TEST(CreatureAttention, TheShareOfItsLifeItWasLookedAt)
{
	EXPECT_DOUBLE_EQ(Share(0, 0), 0.0);
	EXPECT_DOUBLE_EQ(Share(5, 0), 0.0);
	EXPECT_DOUBLE_EQ(Share(25, 100), 0.25);
	EXPECT_DOUBLE_EQ(Share(100, 100), 1.0);
	// divided at a float's precision: a third is the float nearest a third
	EXPECT_EQ(Share(1, 3), static_cast<double>(1.0f / 3.0f));
	// the lived turns are divided as a signed number: past the most a signed count holds, the share is below nothing.
	// A share just below nothing is cut to the least word, a share of minus one is the most
	EXPECT_LT(Share(3, 0x80000001u), 0.0);
	EXPECT_EQ(HowMuch(Share(3, 0x80000001u)), 0);
	EXPECT_EQ(Share(0x80000000u, 0x80000001u), -1.0);
	EXPECT_EQ(HowMuch(Share(0x80000000u, 0x80000001u)), 4);
}

TEST(CreatureAttention, FiveWordsForHowMuch)
{
	EXPECT_EQ(HowMuch(0.0), 0);
	EXPECT_EQ(HowMuch(0.19), 0);
	EXPECT_EQ(HowMuch(0.2), 1);
	EXPECT_EQ(HowMuch(0.79), 3);
	EXPECT_EQ(HowMuch(0.8), 4);
	EXPECT_EQ(HowMuch(1.0), 4);
	EXPECT_EQ(HowMuch(3.0), 4);
	// The game counts without a sign, so below nothing is the most
	EXPECT_EQ(HowMuch(-0.5), 4);
	// A fifth just short of a whole one is the lower word
	EXPECT_EQ(HowMuch(Share(1, 5)), 1);
}

TEST(CreatureAttention, HowMuchIsWorkedOutAtAFloatsPrecision)
{
	// 10,000,000 of 50,000,001 turns is just short of a fifth: in double, times five, it stays just short of one, but
	// the share and the product each round to a float, which makes it one
	EXPECT_LT(10'000'000.0 / 50'000'001.0 * 5.0, 1.0);
	EXPECT_EQ(HowMuch(Share(10'000'000, 50'000'001)), 1);
}

TEST(CreatureAttention, ATurnLookedAtStartsTheTurnsNotLookedAtAgain)
{
	uint32_t lookedAt = 3;
	uint32_t notLookedAt = 0;
	NoteTurn(false, lookedAt, notLookedAt);
	NoteTurn(false, lookedAt, notLookedAt);
	EXPECT_EQ(lookedAt, 3u);
	EXPECT_EQ(notLookedAt, 2u);
	NoteTurn(true, lookedAt, notLookedAt);
	EXPECT_EQ(lookedAt, 4u);
	EXPECT_EQ(notLookedAt, 0u);
}

TEST(CreatureAttention, WholeSecondsAtWholeTurnsASecond)
{
	EXPECT_EQ(WholeSeconds(0, 100), 0u);
	EXPECT_EQ(WholeSeconds(9, 100), 0u);
	EXPECT_EQ(WholeSeconds(10, 100), 1u);
	EXPECT_EQ(WholeSeconds(605, 100), 60u);
	// at 30 ms a turn the turns a second are cut to 33
	EXPECT_EQ(WholeSeconds(65, 30), 1u);
	EXPECT_EQ(WholeSeconds(66, 30), 2u);
	// no whole turn a second, or no time a turn: none
	EXPECT_EQ(WholeSeconds(5000, 1001), 0u);
	EXPECT_EQ(WholeSeconds(5000, 0), 0u);
}

TEST(CreatureAttention, LonelinessGrowsInWholeSecondsAndIsWholeAfterAMinute)
{
	// a minute's share of each second is the float nearest a sixtieth
	EXPECT_EQ(std::bit_cast<uint32_t>(1.0f / 60.0f), 0x3C888889u);
	EXPECT_EQ(Loneliness(0, 100), 0.0f);
	// part of a second counts nothing
	EXPECT_EQ(Loneliness(19, 100), 1.0f * (1.0f / 60.0f));
	EXPECT_EQ(Loneliness(300, 100), 30.0f * (1.0f / 60.0f));
	EXPECT_EQ(Loneliness(599, 100), 59.0f * (1.0f / 60.0f));
	EXPECT_EQ(Loneliness(600, 100), 1.0f);
	EXPECT_EQ(Loneliness(0xFFFFFFFFu, 100), 1.0f);
}

TEST(CreatureAttention, LackOfInteractionGrowsInWholeSecondsAndIsWholeAfterTwoMinutes)
{
	// two minutes' share of each second is the float nearest a hundred and twentieth
	EXPECT_EQ(std::bit_cast<uint32_t>(1.0f / 120.0f), 0x3C088889u);
	EXPECT_EQ(LackOfInteraction(0, 100), 0.0f);
	EXPECT_EQ(LackOfInteraction(9, 100), 0.0f);
	EXPECT_EQ(LackOfInteraction(605, 100), 60.0f * (1.0f / 120.0f));
	EXPECT_EQ(LackOfInteraction(1199, 100), 119.0f * (1.0f / 120.0f));
	EXPECT_EQ(LackOfInteraction(1200, 100), 1.0f);
	EXPECT_EQ(LackOfInteraction(5000, 0), 0.0f);
}

TEST(CreatureAttention, IgnoredOnceMoreThanTwoMinutesOfTurnsGoByUnseen)
{
	EXPECT_FALSE(IgnoredByPlayer(0, 100));
	EXPECT_FALSE(IgnoredByPlayer(1200, 100));
	EXPECT_TRUE(IgnoredByPlayer(1201, 100));
	// at 30 ms a turn, 33 whole turns a second
	EXPECT_FALSE(IgnoredByPlayer(3960, 30));
	EXPECT_TRUE(IgnoredByPlayer(3961, 30));
	// with no whole turn a second any turn unseen is too many; with no time a turn, never
	EXPECT_TRUE(IgnoredByPlayer(1, 1001));
	EXPECT_FALSE(IgnoredByPlayer(1, 0));
}
