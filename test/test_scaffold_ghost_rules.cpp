/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <bit>

#include <gtest/gtest.h>

#include "ECS/ScaffoldGhostRules.h"

using openblack::ecs::scaffold_ghost::HeldAlpha;
using openblack::ecs::scaffold_ghost::HeldPhantom;
using openblack::ecs::scaffold_ghost::HeldPoseOf;

TEST(ScaffoldGhostRules, TheNewGhostGrowsInOverTheFadeWithEachStepRoundedToAFloat)
{
	// The timer 400 just set, 30 ms into the turn: 370 left of 400, so 1 - 370 x 0.0025 of its size, which the float
	// steps make 0.07500005 rather than 0.075
	const auto first = HeldPoseOf({.fade = 400, .appearing = true, .scale = 1.0f, .yAngle = 0.0f}, 1230, 0.0f);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(first->scale), 0x3D9999A0U);
	// A turn on (the timer 300) at the turn's start: a quarter grown, times the phantom's own size
	const auto later = HeldPoseOf({.fade = 300, .appearing = true, .scale = 1.2f, .yAngle = 0.0f}, 2000, 0.0f);
	ASSERT_TRUE(later.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(later->scale), 0x3E99999AU);
	// Grown: no timer left, its full size
	const auto grown = HeldPoseOf({.fade = 0, .appearing = true, .scale = 1.2f, .yAngle = 0.0f}, 2050, 0.0f);
	ASSERT_TRUE(grown.has_value());
	EXPECT_EQ(grown->scale, 1.2f);
}

TEST(ScaffoldGhostRules, TheOldGhostShrinksAwayAndIsNotDrawnOnceNothingIsLeft)
{
	// 250 left: the timer times its size, then times 0.0025
	const auto shrinking = HeldPoseOf({.fade = 300, .appearing = false, .scale = 0.8f, .yAngle = 0.0f}, 1250, 0.0f);
	ASSERT_TRUE(shrinking.has_value());
	EXPECT_EQ(shrinking->scale, 0.5f);
	// The timer no more than the part of the turn: nothing left, and a shrinking ghost is gone
	EXPECT_FALSE(HeldPoseOf({.fade = 70, .appearing = false, .scale = 0.8f}, 570, 0.0f).has_value());
	EXPECT_FALSE(HeldPoseOf({.fade = 0, .appearing = false, .scale = 0.8f}, 0, 0.0f).has_value());
	// One ms more on the timer still draws it, at 1 / 400 of its size
	const auto last = HeldPoseOf({.fade = 71, .appearing = false, .scale = 1.0f}, 570, 0.0f);
	ASSERT_TRUE(last.has_value());
	EXPECT_EQ(last->scale, 1.0f * std::bit_cast<float>(0x3B23D70AU));
}

TEST(ScaffoldGhostRules, ATimerBelowZeroLeavesNothingAsTheUnsignedCompareDoes)
{
	// Compared unsigned, the part of the turn is the smaller, and what is left is below zero
	EXPECT_FALSE(HeldPoseOf({.fade = -50, .appearing = false, .scale = 1.0f}, 10, 0.0f).has_value());
	const auto grown = HeldPoseOf({.fade = -50, .appearing = true, .scale = 0.9f}, 10, 0.0f);
	ASSERT_TRUE(grown.has_value());
	EXPECT_EQ(grown->scale, 0.9f);
}

TEST(ScaffoldGhostRules, BetweenTurnsTheGhostTurnsOnByTheShareOfTheTurnDrawn)
{
	// 45 ms into the turn: 45 x 0.01 of a turn's 0.06, on from the turn's angle 1.5
	const auto pose = HeldPoseOf({.fade = 0, .appearing = true, .scale = 1.0f, .yAngle = 1.5f}, 2345, 0.06f);
	ASSERT_TRUE(pose.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(pose->yAngle), 0x3FC374BCU);
	// The last ms of a turn: the drawn angle may pass 2 pi; only the hand's turn wraps it
	const auto end = HeldPoseOf({.fade = 0, .appearing = true, .scale = 1.0f, .yAngle = 6.25f}, 99, 0.06f);
	ASSERT_TRUE(end.has_value());
	EXPECT_EQ(std::bit_cast<uint32_t>(end->yAngle), 0x40C9E69BU);
	// At the turn's start it stands at the turn's angle; a still phantom (no spin) never turns between turns
	EXPECT_EQ(HeldPoseOf({.fade = 0, .appearing = true, .yAngle = 1.5f}, 3000, 0.06f)->yAngle, 1.5f);
	EXPECT_EQ(HeldPoseOf({.fade = 0, .appearing = true, .yAngle = 1.5f}, 3099, 0.0f)->yAngle, 1.5f);
}

TEST(ScaffoldGhostRules, TheGhostsAlphaIsTwoThirdsOfTheLandLightsTruncated)
{
	// On the map the land light's alpha is 255: 170
	EXPECT_EQ(HeldAlpha(0xFF808080U), 170);
	EXPECT_EQ(HeldAlpha(0x80FFFFFFU), 85);
	// 3 x 0.6666667 is a hair above 2; 1 x it truncates to 0
	EXPECT_EQ(HeldAlpha(0x03000000U), 2);
	EXPECT_EQ(HeldAlpha(0x01FFFFFFU), 0);
	EXPECT_EQ(HeldAlpha(0x00FFFFFFU), 0);
}
