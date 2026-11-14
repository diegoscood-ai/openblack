/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a creature draws a miracle's gesture in the air: where it points for a point of the shape, pinned to the bits the
// original gives on the same inputs, and the turns of the drawing, from the wait for its arm to the end of the hold

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureCastGesture.h"
#include "Magic/Gestures/GestureIcons.h"

using namespace openblack;
using creature_cast_gesture::Outcome;

namespace
{
struct PointCase
{
	float t;
	float radius;
	float heading;
	glm::vec3 head;
	std::array<uint32_t, 3> bits;
};

// A shape of three points, taken a share of the way along, for creatures of two radii facing three ways with their
// heads at two places; the bits are what the original's own function gave for each
const std::array k_PointCases {
    PointCase {0.0f, 7.5f, 0.3f, {12.5f, 30.25f, -40.75f}, {0x418002C4, 0x42183333, 0xC25D7904}},
    PointCase {0.0f, 3.3f, 0.3f, {1003.3f, 52.1f, 2201.7f}, {0x447B35CC, 0x425E20C4, 0x4509344A}},
    PointCase {0.37f, 7.5f, 0.3f, {12.5f, 30.25f, -40.75f}, {0x413DD9E8, 0x4203FF7D, 0xC2629707}},
    PointCase {0.37f, 7.5f, -2.1f, {1003.3f, 52.1f, 2201.7f}, {0x447841E5, 0x425B65E3, 0x450A5D9E}},
    PointCase {0.37f, 3.3f, 0.3f, {1003.3f, 52.1f, 2201.7f}, {0x447AC156, 0x42553D37, 0x45092B48}},
    PointCase {1.0f, 7.5f, -2.1f, {12.5f, 30.25f, -40.75f}, {0x3FC2A83B, 0x42144CCD, 0xC1EE7C82}},
    PointCase {1.0f, 7.5f, 4.0f, {12.5f, 30.25f, -40.75f}, {0x406C9E7C, 0x42144CCD, 0xC1DFF357}},
    PointCase {1.0f, 3.3f, 4.0f, {1003.3f, 52.1f, 2201.7f}, {0x4479DB50, 0x425C6978, 0x4509F500}},
};

const std::vector<glm::vec3>& Shape()
{
	static const std::vector<glm::vec3> k_Shape {{0.1f, 0.0f, 0.2f}, {0.7f, 0.0f, 0.9f}, {0.4f, 0.0f, 0.3f}};
	return k_Shape;
}
} // namespace

TEST(CreatureCastGesture, ThePointIsTheOriginalsToTheBit)
{
	for (const auto& c : k_PointCases)
	{
		const auto point =
		    creature_cast_gesture::PointOf(magic::gestures::ShapePointAt(Shape(), c.t), c.radius, c.heading, c.head);
		EXPECT_EQ(std::bit_cast<uint32_t>(point.x), c.bits[0]) << c.t << " " << c.radius << " " << c.heading;
		EXPECT_EQ(std::bit_cast<uint32_t>(point.y), c.bits[1]) << c.t << " " << c.radius << " " << c.heading;
		EXPECT_EQ(std::bit_cast<uint32_t>(point.z), c.bits[2]) << c.t << " " << c.radius << " " << c.heading;
	}
}

TEST(CreatureCastGesture, TheCircleTakesThirtyTurns)
{
	EXPECT_EQ(creature_cast_gesture::ShareOfATurn(1.0f), 1.0f / 30.0f);
	EXPECT_EQ(creature_cast_gesture::ShareOfATurn(2.0f), 1.0f / 60.0f);
}

TEST(CreatureCastGesture, ItWaitsForItsArmAndEightTurnsThenDrawsAndHoldsEightMore)
{
	float progress = 0.0f;
	uint32_t turns = 0;
	// The step's own count goes on before each turn
	const auto turn = [&](bool pointing) {
		++turns;
		return creature_cast_gesture::Turn(progress, turns, 4, pointing, 0.25f);
	};
	for (int i = 1; i < 8; ++i)
	{
		EXPECT_EQ(turn(true), Outcome::Waiting) << i;
	}
	EXPECT_EQ(progress, 0.0f);
	// Eight turns in, its arm pointing, the point moves on, the count going back to 0 while the shape is not yet drawn
	EXPECT_EQ(turn(true), Outcome::Drawing);
	EXPECT_EQ(progress, 0.25f);
	EXPECT_EQ(turns, 0u);
	EXPECT_EQ(turn(false), Outcome::Drawing);
	EXPECT_EQ(turn(true), Outcome::Drawing);
	EXPECT_EQ(turns, 0u);
	// The shape drawn, it holds until the count is past eight
	EXPECT_EQ(turn(true), Outcome::Drawing);
	EXPECT_EQ(progress, 1.0f);
	EXPECT_EQ(turns, 1u);
	for (uint32_t i = 2; i <= 8; ++i)
	{
		EXPECT_EQ(turn(true), Outcome::Drawing) << i;
	}
	EXPECT_EQ(turn(true), Outcome::Done);
	EXPECT_EQ(progress, 1.0f);
}

TEST(CreatureCastGesture, WithoutItsArmPointingItDoesNotStart)
{
	float progress = 0.0f;
	uint32_t turns = 20;
	EXPECT_EQ(creature_cast_gesture::Turn(progress, turns, 4, false, 0.25f), Outcome::Waiting);
	EXPECT_EQ(progress, 0.0f);
	EXPECT_EQ(turns, 20u);
}

TEST(CreatureCastGesture, AGesturePastTheLastFailsOnceItWouldStart)
{
	float progress = 0.0f;
	uint32_t turns = 3;
	EXPECT_EQ(creature_cast_gesture::Turn(progress, turns, 24, true, 0.25f), Outcome::Waiting);
	turns = 8;
	EXPECT_EQ(creature_cast_gesture::Turn(progress, turns, 24, true, 0.25f), Outcome::Failed);
	EXPECT_EQ(progress, 0.0f);
	turns = 8;
	EXPECT_EQ(creature_cast_gesture::Turn(progress, turns, 23, true, 0.25f), Outcome::Drawing);
}
