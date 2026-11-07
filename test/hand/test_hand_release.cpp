/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What happens to a thing the hand lets go of: where it may stand (physics::from_hand::StandsWhereLetGo,
// TooSteepToStand) and the twist the hand gives it after a throw (ReleaseSpinTorque)

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Physics/FromHand.h"
#include "Hand/HandGrabRules.h"
#include "support/LandFakes.h"

using namespace openblack;
namespace from_hand = openblack::ecs::physics::from_hand;

TEST(FromHandLanding, AThingPutDownStandsOnDryLandOrWhereTheNearestCellIsAboveAltitudeOne)
{
	test::WaterCellIsland island(2);
	// dry land: altitude 4 or more, whatever the water bit
	island.Cell({3, 4}) = test::MakeWaterCell(4, true, false);
	EXPECT_TRUE(from_hand::StandsWhereLetGo(island, {31.0f, 0.0f, 41.0f}));
	// the sea's edge: low cells stand only above altitude 1
	island.Cell({5, 5}) = test::MakeWaterCell(1, false, false);
	EXPECT_FALSE(from_hand::StandsWhereLetGo(island, {51.0f, 0.0f, 51.0f}));
	island.Cell({6, 6}) = test::MakeWaterCell(2, true, false);
	EXPECT_TRUE(from_hand::StandsWhereLetGo(island, {61.0f, 0.0f, 61.0f}));
	// the second test reads the nearest cell, rounded: the point's own cell (7, 7) is sea, the nearest (8, 7) is not
	island.Cell({7, 7}) = test::MakeWaterCell(0, true, false);
	island.Cell({8, 7}) = test::MakeWaterCell(3, false, false);
	EXPECT_TRUE(from_hand::StandsWhereLetGo(island, {78.0f, 0.0f, 71.0f}));
	EXPECT_FALSE(from_hand::StandsWhereLetGo(island, {72.0f, 0.0f, 71.0f}));
	// off the map it falls in
	EXPECT_FALSE(from_hand::StandsWhereLetGo(island, {-5.0f, 0.0f, -5.0f}));
	EXPECT_FALSE(from_hand::StandsWhereLetGo(island, {1000.0f, 0.0f, 1000.0f}));
}

TEST(FromHandLanding, GroundSteeperThanAboutFortyFiveDegreesIsTooSteepToStandOn)
{
	EXPECT_FALSE(from_hand::TooSteepToStand({0.0f, 1.0f, 0.0f}));
	EXPECT_FALSE(from_hand::TooSteepToStand({0.714f, 0.7f, 0.0f}));
	EXPECT_TRUE(from_hand::TooSteepToStand({0.72f, 0.69f, 0.0f}));
}

TEST(FromHandSpin, TheTwistIsOnePointSixTimesTheMassAndSpeedAboutTheLevelAxisAcrossTheHandsMotion)
{
	// speed 5 (3, 0, 4), mass 2: 1.6 x 2 x 5 = 16
	const glm::vec3 velocity {3.0f, 0.0f, 4.0f};
	const auto alongX = from_hand::ReleaseSpinTorque(velocity, 2.0f, {1.0f, 0.0f, 0.0f});
	EXPECT_FLOAT_EQ(alongX.x, 0.0f);
	EXPECT_FLOAT_EQ(alongX.y, 0.0f);
	EXPECT_FLOAT_EQ(alongX.z, -16.0f);
	const auto alongZ = from_hand::ReleaseSpinTorque(velocity, 2.0f, {0.0f, 0.0f, 1.0f});
	EXPECT_FLOAT_EQ(alongZ.x, 16.0f);
	EXPECT_FLOAT_EQ(alongZ.z, 0.0f);
	// the hand's height plays no part, and a body at rest gets no twist
	EXPECT_EQ(from_hand::ReleaseSpinTorque(velocity, 2.0f, {0.0f, 3.0f, 0.0f}), glm::vec3(0.0f));
	EXPECT_EQ(from_hand::ReleaseSpinTorque(glm::vec3(0.0f), 2.0f, {1.0f, 0.0f, 1.0f}), glm::vec3(0.0f));
	EXPECT_FLOAT_EQ(hand_grab::k_ReleaseSpinFactor, 1.6f);
}
