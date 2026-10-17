/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's HandMotion tests, from his test_magic_foundation.cpp and test_miracle_visuals.cpp, over our formulas

#include <array>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Magic/HandMotion.h"

using namespace openblack::magic;

namespace
{
constexpr float k_Epsilon = 1e-4f;

// His food and wood, and water, pours. Ours takes these from each miracle's sprinkle file
constexpr PourSettings k_FoodWoodPour {
    .totalTime = 4.0f, .heightToRaise = 10.0f, .angleToRaise = 1.07257f, .loops = true, .clampHand = true};
constexpr PourSettings k_WaterPour {
    .totalTime = 8.0f, .heightToRaise = 8.0f, .angleToRaise = 0.0f, .loops = true, .clampHand = false};
} // namespace

TEST(HandMotion, TheHandsMovementGetsFourFifthsOfTheWayInATenthOfASecond)
{
	const glm::vec3 raw {100.0f, 0.0f, 0.0f};
	EXPECT_NEAR(FilterHandVelocity(glm::vec3(0.0f), raw, 0.1f).x, 80.0f, 0.01f);
	// The same in many small steps
	glm::vec3 smoothed(0.0f);
	for (int i = 0; i < 10; ++i)
	{
		smoothed = FilterHandVelocity(smoothed, raw, 0.01f);
	}
	EXPECT_NEAR(smoothed.x, 80.0f, 0.01f);
	EXPECT_EQ(FilterHandVelocity(glm::vec3(3.0f), raw, 0.0f), glm::vec3(3.0f));
}

TEST(HandMotion, ANaturalSplinePassesThroughItsPointsWithoutBendingAtTheEnds)
{
	const CubicSpline curve(k_PourKeyPoints);
	for (const auto& point : k_PourKeyPoints)
	{
		EXPECT_NEAR(curve(point.x), point.y, k_Epsilon);
	}
	// Symmetric, and rising above the held points between them
	EXPECT_NEAR(curve(0.1f), curve(0.9f), k_Epsilon);
	EXPECT_NEAR(curve(0.5f), 1.6136f, 1e-3f);
	// A straight line stays one
	const std::array line {glm::vec2 {0.0f, 0.0f}, glm::vec2 {1.0f, 2.0f}, glm::vec2 {3.0f, 6.0f}};
	EXPECT_NEAR(CubicSpline(line)(2.0f), 4.0f, k_Epsilon);
}

TEST(HandMotion, ThePoursCurveIsFlatAtBothEndsAndSwellsToAlmostTwiceItsPoints)
{
	const CubicSpline curve(k_PourKeyPoints, 0.0f, 0.0f);
	for (const auto& point : k_PourKeyPoints)
	{
		EXPECT_NEAR(curve(point.x), point.y, k_Epsilon);
	}
	EXPECT_NEAR(curve(0.1f), 0.3393f, 1e-3f);
	EXPECT_NEAR(curve(0.9f), 0.3393f, 1e-3f);
	EXPECT_NEAR(curve(0.5f), 1.9643f, 1e-3f);
	// Flat at the ends: barely moving a hair in
	EXPECT_NEAR(curve(0.001f), 0.0f, 1e-4f);
}

TEST(HandMotion, FoodAndWoodLiftTheHandOverFourSecondsAndStartOver)
{
	PourState pour;
	StartPour(pour, k_FoodWoodPour);
	EXPECT_TRUE(pour.active);
	// Eight tenths of a second in, the curve's first key point: up ten, tipped 61 degrees
	for (int turn = 0; turn < 8; ++turn)
	{
		StepPour(pour, 0.1f);
	}
	EXPECT_NEAR(pour.current.raise, 10.0f, 1e-3f);
	EXPECT_NEAR(pour.current.tilt, 1.07257f, 1e-3f);
	// Halfway through, at the top of the curve: up 16.1 and tipped 99 degrees
	for (int turn = 8; turn < 20; ++turn)
	{
		StepPour(pour, 0.1f);
	}
	// Changed from his 19.643 and 2.1068 (a curve with flat ends): ours is the natural curve the game uses, which
	// swells to 1.6136 times the held points
	EXPECT_NEAR(pour.current.raise, 16.136f, 1e-2f);
	EXPECT_NEAR(pour.current.tilt, 1.7307f, 1e-3f);
	// Halfway between turns it is halfway between their poses
	const auto half = PourPoseAt(pour, 0.5f);
	EXPECT_NEAR(half.raise, (pour.previous.raise + pour.current.raise) * 0.5f, k_Epsilon);
	// At the end it is down again, and it starts over
	for (int turn = 20; turn < 40; ++turn)
	{
		StepPour(pour, 0.1f);
	}
	EXPECT_NEAR(pour.current.raise, 0.0f, 1e-2f);
	StepPour(pour, 0.1f);
	EXPECT_TRUE(pour.active);
	StepPour(pour, 0.1f);
	EXPECT_GT(pour.current.raise, 0.0f);
	// Stopped, it comes back to rest over the next turn from where it was drawn
	const float before = pour.current.raise;
	StopPour(pour);
	EXPECT_FLOAT_EQ(PourPoseAt(pour, 1.0f).raise, 0.0f);
	EXPECT_FLOAT_EQ(PourPoseAt(pour, 0.0f).raise, pour.previous.raise);
	EXPECT_NEAR(PourPoseAt(pour, 0.5f).raise, pour.previous.raise * 0.5f, k_Epsilon);
	EXPECT_GT(before, 0.0f);
	StepPour(pour, 0.1f);
	EXPECT_FLOAT_EQ(PourPoseAt(pour, 0.0f).raise, 0.0f);
}

TEST(HandMotion, APourThatDoesntLoopStops)
{
	PourState pour;
	StartPour(pour, {.totalTime = 1.0f, .heightToRaise = 4.0f, .angleToRaise = 1.0f, .loops = false});
	for (int turn = 0; turn < 12; ++turn)
	{
		StepPour(pour, 0.1f);
	}
	EXPECT_FALSE(pour.active);
}

TEST(HandMotion, APourThatClampsTheHandKeepsItWhereItBegan)
{
	PourState pour;
	StartPour(pour, k_FoodWoodPour, {1.0f, 2.0f, 3.0f});
	StepPour(pour, 0.1f);
	ASSERT_TRUE(PourPoseAt(pour, 1.0f).pinned.has_value());
	EXPECT_NEAR(PourPoseAt(pour, 1.0f).pinned->z, 3.0f, k_Epsilon);
	StopPour(pour);
	EXPECT_FALSE(PourPoseAt(pour, 1.0f).pinned.has_value());
	StartPour(pour, k_WaterPour, {1.0f, 2.0f, 3.0f});
	EXPECT_FALSE(PourPoseAt(pour, 1.0f).pinned.has_value());
	// The water lifts the hand without tipping it, over eight seconds
	StepPour(pour, 1.6f);
	EXPECT_NEAR(PourPoseAt(pour, 1.0f).raise, 8.0f, 1e-3f);
	EXPECT_NEAR(PourPoseAt(pour, 1.0f).tilt, 0.0f, k_Epsilon);
}
