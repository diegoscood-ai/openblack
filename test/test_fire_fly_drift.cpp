/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How far a firefly's orbit swings out round its point (ecs::fire_flies::DriftAmplitude), pinned to the original's
// values: the fifths and their factors are the original's floats.

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/FireFlies.h"

using namespace openblack::ecs::fire_flies;

TEST(FireFlies, TheDriftIsNoneAsleepAndWholeHovering)
{
	EXPECT_EQ(DriftAmplitude(State::Asleep, 0.5f), 0.0f);
	EXPECT_EQ(DriftAmplitude(State::Hovering, 0.5f), 1.0f);
}

TEST(FireFlies, TheDriftGrowsOverTheFirstFifthOut)
{
	EXPECT_EQ(DriftAmplitude(State::FlyingOut, 0.0f), 0.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingOut, 0.1f), 0.1f * 5.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingOut, 0.2f), 1.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingOut, 0.9f), 1.0f);
}

TEST(FireFlies, TheDriftStaysWholeBeforeTheLastFifthHome)
{
	EXPECT_EQ(DriftAmplitude(State::FlyingHome, 0.0f), 1.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingHome, 0.79f), 1.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingHome, 0.8f), 1.0f);
}

TEST(FireFlies, TheDriftDiesOverTheLastFifthHomeByTheOriginalsFactor)
{
	// the original's factor is the float just over 5
	const float factor = std::bit_cast<float>(0x40A00001u);
	EXPECT_EQ(DriftAmplitude(State::FlyingHome, 0.9f), 1.0f - (0.9f - 0.8f) * factor);
	EXPECT_NE(DriftAmplitude(State::FlyingHome, 0.9f), 1.0f - (0.9f - 0.8f) * 5.0f);
	EXPECT_EQ(DriftAmplitude(State::FlyingHome, 1.0f), 1.0f - (1.0f - 0.8f) * factor);
}

TEST(FireFlies, AFlightTakesItsLengthOverThreeMetresASecondTimesItsSpeedAtLeastHalfASecond)
{
	EXPECT_FLOAT_EQ(FlightSeconds(30.0f, 2.0f), 5.0f);
	EXPECT_FLOAT_EQ(FlightSeconds(1.5f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(FlightSeconds(1.0f, 1.0f), 0.5f);
	EXPECT_FLOAT_EQ(FlightSeconds(0.0f, 1.0f), 0.5f);
}

TEST(FireFlies, OneOutIsDrawnAt190WithinAHundredMetresFadingToNoneAtThreeHundred)
{
	EXPECT_EQ(Alpha(false, 5000.0f), 190.0f);
	EXPECT_EQ(Alpha(false, 10000.0f), 190.0f);
	EXPECT_EQ(Alpha(false, 50000.0f), 95.0f);
	// truncated to a whole step, as the original stores it in a byte: 166.25 is drawn at 166
	EXPECT_EQ(Alpha(false, 20000.0f), 166.0f);
	EXPECT_EQ(Alpha(false, 90000.0f), 0.0f);
	EXPECT_EQ(Alpha(false, 95000.0f), 0.0f);
	// one asleep is never drawn
	EXPECT_EQ(Alpha(true, 5000.0f), 0.0f);
}
