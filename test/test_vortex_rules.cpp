/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "Magic/VortexRules.h"

using namespace openblack;
using namespace openblack::vortex;

TEST(VortexRules, OnlyTheLandVorticesLevelTheGround)
{
	EXPECT_TRUE(LevelsGround(VortexType::In));
	EXPECT_TRUE(LevelsGround(VortexType::Out));
	EXPECT_FALSE(LevelsGround(VortexType::Volcano));
}

TEST(VortexRules, TheSquareIsElevenCellsRoundTheVortexsCell)
{
	EXPECT_EQ(k_LevelCellCount, 121u);
	const auto centre = CentreCell({123.9f, 45.2f});
	EXPECT_EQ(centre, glm::ivec2(12, 4));
	EXPECT_EQ(SquareCell(centre, 0), glm::ivec2(7, -1));
	// Along z first
	EXPECT_EQ(SquareCell(centre, 1), glm::ivec2(7, 0));
	EXPECT_EQ(SquareCell(centre, 11), glm::ivec2(8, -1));
	EXPECT_EQ(SquareCell(centre, 60), glm::ivec2(12, 4));
	EXPECT_EQ(SquareCell(centre, 120), glm::ivec2(17, 9));
}

TEST(VortexRules, TheAverageCountsEveryCell)
{
	const std::array<uint8_t, 4> altitudes {10, 20, 0, 30};
	EXPECT_FLOAT_EQ(AverageAltitude(altitudes), 15.0f);
	EXPECT_FLOAT_EQ(AverageAltitude({}), 0.0f);
}

TEST(VortexRules, TheLandIsPulledWhollyWithinFiftyAndLessOutToFiftySix)
{
	EXPECT_FLOAT_EQ(LevelWeight(0.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(49.9f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(50.0f), 1.0f);
	EXPECT_FLOAT_EQ(LevelWeight(53.0f), 0.5f);
	EXPECT_FLOAT_EQ(LevelWeight(56.0f), 0.0f);
	EXPECT_FLOAT_EQ(LevelWeight(57.0f), 0.0f);
}

TEST(VortexRules, ACellIsPulledTowardsTheAverageByTheAmount)
{
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 0.0f), 100);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 0.5f), 75);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 10.0f, 1.0f), 50);
	// Halfway between 50 and 56 it goes half as far
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 53.0f, 1.0f), 75);
	EXPECT_EQ(LevelledAltitude(100, 50.0f, 60.0f, 1.0f), 100);
	// Rounded to the nearest, halves to the even height
	EXPECT_EQ(LevelledAltitude(10, 11.0f, 0.0f, 0.5f), 10);
	EXPECT_EQ(LevelledAltitude(11, 12.0f, 0.0f, 0.5f), 12);
	EXPECT_EQ(LevelledAltitude(0, 255.0f, 0.0f, 1.0f), 255);
}

TEST(VortexRules, TheSquareFlattensRoundItsMiddleAndLeavesItsCorners)
{
	// A peak in the middle of a square of height 10, the vortex on the peak's cell
	std::vector<uint8_t> originals(k_LevelCellCount, 10);
	originals[60] = 131;
	const auto average = AverageAltitude(originals);
	EXPECT_FLOAT_EQ(average, 11.0f);
	const glm::vec2 centre {120.0f, 40.0f};
	const auto levelled = LevelSquare(centre, originals, average, 1.0f);
	// The peak and the land within 50 come to the average
	EXPECT_EQ(levelled[60], 11);
	EXPECT_EQ(levelled[61], 11);
	// A corner, 70 away, keeps its height
	EXPECT_EQ(levelled[0], 10);
	EXPECT_EQ(levelled[120], 10);
	// Not levelled at all: the heights are those of the start
	const auto untouched = LevelSquare(centre, originals, average, 0.0f);
	EXPECT_EQ(untouched[60], 131);
	EXPECT_EQ(untouched[0], 10);
}

TEST(VortexRules, TheLevelledHeightFollowsTheDistanceFromTheMiddle)
{
	// Within 50 the whole way to the average by the amount: 10 + 10 x 0.75, the half rounded to the even 18
	EXPECT_EQ(LevelledAltitude(10, 20.0f, 30.0f, 0.75f), 18);
	EXPECT_EQ(LevelledAltitude(30, 20.0f, 0.0f, 1.0f), 20);
	// From 50 to 56 a straight fall: 10 + 3 x 12 / 6 x 0.5
	EXPECT_EQ(LevelledAltitude(10, 22.0f, 53.0f, 0.5f), 13);
	EXPECT_EQ(LevelledAltitude(10, 22.0f, 56.0f, 1.0f), 10);
	// Beyond 56 the land stays as it was: a cell 40 across and 50 along from the middle
	EXPECT_EQ(LevelledAltitude(10, 22.0f, std::sqrt(40.0f * 40.0f + 50.0f * 50.0f), 1.0f), 10);
}

TEST(VortexRules, TheHeightIsRoundedFromFloatStepsAsTheGameDoes)
{
	// Each step rounded to a float, as the game's are. Worked out in double and rounded once, these would be 59 and 5
	EXPECT_EQ(LevelledAltitude(0, 7854.0f / 121.0f, 50.5f, 1.0f), 60);
	EXPECT_EQ(LevelledAltitude(18, 3.0f, 10.0f, 0.9f), 4);
}
