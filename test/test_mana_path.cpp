/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mana path the hand lets out past the border: its maths (the rule, with the game's file, comes with the rule)

#include <cmath>

#include <numbers>

#include <gtest/gtest.h>

#include "Particles/ManaPathMaths.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::mana_path;

namespace
{
constexpr Settings k_Steady {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = true};
constexpr Settings k_Curved {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = false};
constexpr Draws k_Draws {.frequency = 0.4f, .amplitude = 0.2f, .phase = 0.5f};
constexpr float k_Step = 0.1f;
constexpr float k_Ground = 7.0f;
} // namespace

TEST(ManaPath, APathKnowsItsLengthAcrossTheGroundAndTheWayAcross)
{
	const auto path = MakePath({10.0f, 5.0f, 20.0f}, {13.0f, 50.0f, 24.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(path.length, 5.0f);
	// A quarter turn from the way along (0.6, 0.8)
	EXPECT_FLOAT_EQ(path.side.x, 0.8f);
	EXPECT_FLOAT_EQ(path.side.y, 0.0f);
	EXPECT_FLOAT_EQ(path.side.z, -0.6f);
	// Each spark weaves by its own share of 0.6 to 1.4 of the file's frequency and size, starting along the noise by
	// its draw out of 256
	EXPECT_FLOAT_EQ(path.frequency, 0.5f);
	EXPECT_FLOAT_EQ(path.amplitude, 4.8f);
	EXPECT_FLOAT_EQ(path.phase, 128.0f);
}

TEST(ManaPath, ASparkIsTurnedAlongItsPathAQuarterTurnRound)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Heading(path), std::numbers::pi_v<float>);
}

TEST(ManaPath, ASteadySparkCrossesItsPathAtItsSpeed)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Steady, 1.0f), 0.5f);
	// The point is held to the path's ends
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 0.25f).x, 20.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 1.5f).x, 80.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, -1.0f).x, 0.0f);
}

TEST(ManaPath, ACurvedSparkTakesItsTimeAndLeavesAndArrivesAlongThePath)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {120.0f, 0.0f, 0.0f}, k_Curved, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Curved, 1.5f), 0.5f);
	// Its ends, and with tangents as long as the path the curve runs evenly
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.0f).x, 0.0f, 1e-4f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 1.0f).x, 120.0f, 1e-3f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.25f).x, 30.0f, 1e-3f);
	// The curve is not held to its ends
	EXPECT_GT(PointOnPath(path, k_Curved, 1.2f).x, 120.0f);
}

TEST(ManaPath, TheWeaveGrowsHoldsAndDiesAway)
{
	EXPECT_FLOAT_EQ(WeaveShare(-0.1f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.1f), 0.5f);
	EXPECT_FLOAT_EQ(WeaveShare(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.8f), 1.0f);
	EXPECT_NEAR(WeaveShare(0.9f), 0.5f, 1e-5f);
	EXPECT_NEAR(WeaveShare(1.0f), 0.0f, 1e-5f);
	EXPECT_FLOAT_EQ(WeaveShare(1.1f), 0.0f);
}

TEST(ManaPath, ASparkWeavesAcrossItsPathByTheNoise)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	// The noise is read along the path by its frequency over its length, from its phase
	EXPECT_FLOAT_EQ(WeaveNoiseAt(path, 0.5f), 0.5f * 10.0f * 0.5f + 128.0f);
	// Across a path along z is along x
	const auto woven = Woven(path, {0.0f, 0.0f, 5.0f}, 0.5f, 0.5f);
	EXPECT_FLOAT_EQ(woven.x, 0.5f * 4.8f);
	EXPECT_FLOAT_EQ(woven.y, 5.0f);
	// Not at all where it starts
	const auto start = Woven(path, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(start.x, 0.0f);
}

TEST(ManaPath, TheGroundIsReadOnTheMapsFixedGrid)
{
	// 1 m is 6553.6 steps, truncated to 6553
	EXPECT_FLOAT_EQ(GroundPoint({1.0f, 0.0f}).x, 6553.0f * 10.0f / 65536.0f);
	EXPECT_FLOAT_EQ(GroundPoint({0.0f, 10.0f}).y, 10.0f);
}
