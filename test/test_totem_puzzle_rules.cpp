/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Japanese totem puzzle's rules (ecs::totem_puzzle): the arc, the move table, the counting round in fives and the
// solution. See docs/wiki/world/totem-puzzle.md

#include <cstdint>

#include <array>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/TotemPuzzleRules.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
constexpr map_coords::MapCoords k_Place {.x = 0x04000000, .z = 0x05000000, .altitude = 0.0f};
} // namespace

// The arc's six steps read the sine table a sixth of the doubled step apart, as the unsigned multiplication gives them
TEST(TotemPuzzleRules, TheArcsStepsInTheSineTable)
{
	constexpr std::array<uint32_t, totem_puzzle::k_Totems> k_Indices {0, 170, 341, 512, 682, 853};
	for (size_t totem = 0; totem < totem_puzzle::k_Totems; ++totem)
	{
		EXPECT_EQ(totem_puzzle::ArcIndex(static_cast<uint32_t>(totem) * totem_puzzle::k_ArcStep), k_Indices.at(totem));
	}
	EXPECT_EQ(totem_puzzle::k_ArcEnd, static_cast<int32_t>(totem_puzzle::k_Totems) * totem_puzzle::k_ArcStep);
}

// The first totem stands 20 m along z from the place and the fourth 20 m along x, times the size
TEST(TotemPuzzleRules, TheTotemsStandTwentyMetresTimesTheSizeFromThePlace)
{
	const auto first = totem_puzzle::ArcPlace(k_Place, 1.0f, 0);
	EXPECT_EQ(first.x, k_Place.x);
	EXPECT_EQ(first.z, k_Place.z + 131072);
	const auto fourth = totem_puzzle::ArcPlace(k_Place, 1.0f, 3);
	EXPECT_EQ(fourth.x, k_Place.x + 131072);
	EXPECT_EQ(fourth.z, k_Place.z);
	const auto bigger = totem_puzzle::ArcPlace(k_Place, 1.5f, 0);
	EXPECT_EQ(bigger.z, k_Place.z + 196608);
	EXPECT_EQ(bigger.altitude, k_Place.altitude);
}

// Each totem is turned from the middle: the angle to it less a quarter turn, rounded once to float
TEST(TotemPuzzleRules, EachTotemIsTurnedAQuarterTurnFromTheWayToIt)
{
	EXPECT_EQ(totem_puzzle::FacingFromMiddle(k_Place, totem_puzzle::ArcPlace(k_Place, 1.0f, 3)), -1.5707964f);
	EXPECT_EQ(totem_puzzle::FacingFromMiddle(k_Place, totem_puzzle::ArcPlace(k_Place, 1.0f, 0)),
	          static_cast<float>(1.5707963267948966 - static_cast<double>(1.5707964f)));
}

// Raising the first totem by one moves the second by four and the sixth by two, counted round in fives
TEST(TotemPuzzleRules, AMoveMovesTheOthersByTheTable)
{
	const auto after = totem_puzzle::AfterMove({}, 0, 1);
	EXPECT_EQ(after, (std::array<int32_t, totem_puzzle::k_Totems> {1, 4, 0, 0, 0, 2}));
	// the second totem moves no other: lowered from 4 to 3, it alone changes
	EXPECT_EQ(totem_puzzle::AfterMove(after, 1, 3), (std::array<int32_t, totem_puzzle::k_Totems> {1, 3, 0, 0, 0, 2}));
	// the third raised by two moves the first and second by two and the fourth by four
	EXPECT_EQ(totem_puzzle::AfterMove({}, 2, 2), (std::array<int32_t, totem_puzzle::k_Totems> {2, 2, 2, 4, 0, 0}));
}

// Solved only with every kept height at the highest
TEST(TotemPuzzleRules, SolvedWithEveryTotemAtItsHighest)
{
	std::array<int32_t, totem_puzzle::k_Totems> heights {4, 4, 4, 4, 4, 4};
	EXPECT_TRUE(totem_puzzle::Solved(heights));
	heights.at(5) = 3;
	EXPECT_FALSE(totem_puzzle::Solved(heights));
}
