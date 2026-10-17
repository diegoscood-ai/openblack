/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The pure rules for rocks and for where the physics puts what it lets go of (Physics/ObjectRules), on plain values
// and a fake land. The rock case is the upstream physics objects test's; its other cases wait for the rules they test.

#include <vector>

#include <gtest/gtest.h>

#include "Physics/ObjectRules.h"
#include "support/LandFakes.h"

using namespace openblack::physics::objects;

TEST(PhysicsObjects, OnlyTallRocksBreakWhenTapped)
{
	EXPECT_FALSE(RockBreaksWhenTapped(0.7f));
	EXPECT_TRUE(RockBreaksWhenTapped(0.71f));
}

TEST(PhysicsObjects, WhatThePhysicsLetsGoOfStandsOnTheMapsGrid)
{
	// x and z through the map's 16.16 coordinates: x 6553.6, truncated towards 0, back to metres (x 10 / 65536)
	const auto at = PlacedOnTheMap(nullptr, glm::vec3(1.0f, 2.5f, -3.0f));
	EXPECT_EQ(at.x, 32765.0f / 32768.0f);  // 6553 units
	EXPECT_EQ(at.z, -98300.0f / 32768.0f); // -19660.8 truncated to -19660 units
	// without a land the height is above 0, so it is the point's
	EXPECT_EQ(at.y, 2.5f);
	// a point already on the grid stays where it is
	EXPECT_EQ(PlacedOnTheMap(nullptr, glm::vec3(10.0f, 0.0f, 0.0f)).x, 10.0f);
	EXPECT_EQ(PlacedOnTheMap(nullptr, glm::vec3(2.25f, 0.0f, 0.0f)).x, 73725.0f / 32768.0f); // 14745 units
}

TEST(PhysicsObjects, ItsHeightIsKeptAboveTheLandWhereItStands)
{
	// the land is asked once, at the point on the grid, and the height is the land's there plus the height above it,
	// each in float: 3.3 above a land at 12.7 comes back one float higher
	const openblack::test::HeightFieldIsland land([](glm::vec2) { return 12.7f; });
	const auto at = PlacedOnTheMap(&land, glm::vec3(1.0f, 3.3f, -3.0f));
	ASSERT_EQ(land.asked.size(), 1u);
	EXPECT_EQ(land.asked.front(), glm::vec2(32765.0f / 32768.0f, -98300.0f / 32768.0f));
	const float above = 3.3f - 12.7f;
	EXPECT_EQ(at.y, 12.7f + above);
	EXPECT_NE(at.y, 3.3f);
	// on a sloping land the height is the land's at the point on the grid, not at the point itself
	const openblack::test::HeightFieldIsland slope([](glm::vec2 xz) { return xz.x; });
	const auto onSlope = PlacedOnTheMap(&slope, glm::vec3(1.0f, 5.0f, 0.0f));
	EXPECT_EQ(onSlope.y, 32765.0f / 32768.0f + (5.0f - 32765.0f / 32768.0f));
}
