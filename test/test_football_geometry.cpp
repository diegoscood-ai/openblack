/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The football pitch's own space and its goals (docs/wiki/world/buildings.md#the-football-pitch): the pitch's matrix
// and its inverse, the goal mouth test and the test along the pitch

#include <cstdint>

#include <array>
#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AffineMatrix.h"
#include "ECS/FootballGeometry.h"

namespace geometry = openblack::ecs::football_geometry;
using openblack::affine::AffineMatrix;

namespace
{
/// The goal model's first goal mouth, as Data\football.l3d holds it
constexpr geometry::GoalBox k_FirstGoal {.centre = {13.555267f, 1.3487837f, 0.22755349f},
                                         .halfSize = {0.14197731f, 1.3487837f, 2.6265788f}};
} // namespace

TEST(FootballGeometry, AnUnturnedPitchHasItsScaleOnTheDiagonal)
{
	const auto m = geometry::PitchMatrix({100.0f, 2.0f, 300.0f}, 0.0f, 1.0f).m;
	EXPECT_EQ(m, (std::array {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 100.0f, 2.0f, 300.0f}));
	const auto scaled = geometry::PitchMatrix({1.0f, 2.0f, 3.0f}, 0.0f, 1.5f).m;
	EXPECT_EQ(scaled.at(0), 1.5f);
	EXPECT_EQ(scaled.at(4), 1.5f);
	EXPECT_EQ(scaled.at(8), 1.5f);
	EXPECT_EQ(scaled.at(2), 0.0f);
}

TEST(FootballGeometry, ATurnedPitchTurnsAboutTheVertical)
{
	const auto m = geometry::PitchMatrix({0.0f, 0.0f, 0.0f}, 0.5f, 1.25f).m;
	// c s and s s, each rounded once; the middle row is the scale
	EXPECT_EQ(m.at(0), 1.0969782f);
	EXPECT_EQ(m.at(2), 0.5992819f);
	EXPECT_EQ(m.at(6), -0.5992819f);
	EXPECT_EQ(m.at(8), 1.0969782f);
	EXPECT_EQ(m.at(4), 1.25f);
	EXPECT_EQ(m.at(1), 0.0f);
}

TEST(FootballGeometry, TheInverseIsTheOriginalsInPlaceInverse)
{
	// the original's in-place inverse run on this matrix (the oracle): every cell bit for bit
	const AffineMatrix a {{0.5f, 0.25f, -0.75f, 1.5f, -0.125f, 0.3f, 0.7f, 2.0f, -1.1f, 3.5f, -7.25f, 12.0f}};
	const auto r = geometry::InverseInPlace(a).m;
	constexpr std::array<uint32_t, 12> k_Expected {0x3E637CBC, 0x3F16A229, 0x3C138F2C, 0xBF64B787, 0x3C44BEFA, 0x3F1CC820,
	                                               0xBFBDD431, 0x3ECAE4DE, 0x3E5730CD, 0x4128AA4F, 0xC0D74964, 0x3FF18356};
	for (size_t i = 0; i < r.size(); ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(r.at(i)), k_Expected.at(i)) << i;
	}
}

TEST(FootballGeometry, ABallThroughTheGoalMouthScores)
{
	// straight through the mouth, from in front of the line to behind it
	EXPECT_TRUE(geometry::CrossesGoalMouth({12.0f, 0.2f, 0.0f}, {14.0f, 0.2f, 0.0f}, k_FirstGoal));
	// either way over the line
	EXPECT_TRUE(geometry::CrossesGoalMouth({14.0f, 0.2f, 1.0f}, {12.0f, 0.2f, 1.0f}, k_FirstGoal));
	// not reaching the line, or past it on both turns
	EXPECT_FALSE(geometry::CrossesGoalMouth({11.0f, 0.2f, 0.0f}, {13.0f, 0.2f, 0.0f}, k_FirstGoal));
	EXPECT_FALSE(geometry::CrossesGoalMouth({14.0f, 0.2f, 0.0f}, {15.0f, 0.2f, 0.0f}, k_FirstGoal));
	// over the bar: the mouth is only as high as its half-height
	EXPECT_FALSE(geometry::CrossesGoalMouth({12.0f, 1.5f, 0.0f}, {14.0f, 1.5f, 0.0f}, k_FirstGoal));
	// but below the ground still counts
	EXPECT_TRUE(geometry::CrossesGoalMouth({12.0f, -3.0f, 0.0f}, {14.0f, -3.0f, 0.0f}, k_FirstGoal));
	// wide of either post
	EXPECT_FALSE(geometry::CrossesGoalMouth({12.0f, 0.2f, 3.0f}, {14.0f, 0.2f, 3.0f}, k_FirstGoal));
	EXPECT_FALSE(geometry::CrossesGoalMouth({12.0f, 0.2f, -3.0f}, {14.0f, 0.2f, -3.0f}, k_FirstGoal));
	// starting exactly on the line and coming back over it is a goal wherever it is
	EXPECT_TRUE(geometry::CrossesGoalMouth({13.555267f, 9.0f, 9.0f}, {13.0f, 9.0f, 9.0f}, k_FirstGoal));
}

TEST(FootballGeometry, OnlyTheLengthTellsWhetherTheBallIsAlongThePitch)
{
	EXPECT_TRUE(geometry::AlongThePitch(0.0f, 20.0f));
	EXPECT_TRUE(geometry::AlongThePitch(20.0f, 20.0f));
	EXPECT_TRUE(geometry::AlongThePitch(-20.0f, 20.0f));
	EXPECT_FALSE(geometry::AlongThePitch(20.5f, 20.0f));
	EXPECT_FALSE(geometry::AlongThePitch(-20.5f, 20.0f));
}

TEST(FootballGeometry, AHeadingNeedsTheBallToHaveMoved)
{
	EXPECT_FALSE(geometry::BallHeading({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.00005f}).has_value());
	const auto heading = geometry::BallHeading({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f});
	ASSERT_TRUE(heading.has_value());
	EXPECT_FLOAT_EQ(heading->x, 0.6f);
	EXPECT_FLOAT_EQ(heading->z, 0.8f);
}

TEST(FootballGeometry, AGoalIsPlacedThroughThePitchsDrawnMatrix)
{
	AffineMatrix placement;
	placement.m.at(9) = 100.0f;
	placement.m.at(10) = 5.0f;
	placement.m.at(11) = 200.0f;
	const auto point = geometry::GoalPointInWorld(placement, k_FirstGoal);
	EXPECT_FLOAT_EQ(point.x, 113.555267f);
	EXPECT_FLOAT_EQ(point.y, 6.3487837f);
	EXPECT_FLOAT_EQ(point.z, 200.22755f);
}
