/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Hand/HandFeel.h"

namespace hand_feel = openblack::hand_feel;

TEST(HandFeel, VillagersAndAnimalsAreHoveredBeforeAndModelsFelt)
{
	using hand_feel::Feel;
	EXPECT_EQ(hand_feel::FeelOf({.living = true}), Feel::AtPosition);
	EXPECT_EQ(hand_feel::FeelOf({.living = true, .creature = true, .posedModel = true}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({.posedModel = true}), Feel::AtPosition);
	EXPECT_EQ(hand_feel::FeelOf({.posedModel = true, .animatedStatic = true}), Feel::OnModel);
	EXPECT_EQ(hand_feel::FeelOf({.living = true, .ignored = true}), Feel::Nothing);
}

TEST(HandFeel, HoldingSomethingTheHandFeelsTowardsTheRaisedGround)
{
	const glm::vec3 camera(0.0f, 10.0f, 0.0f);
	const glm::vec3 cursor = glm::normalize(glm::vec3(1.0f, -1.0f, 0.0f));
	EXPECT_EQ(hand_feel::FeelDirection(camera, cursor, std::nullopt, 1.0f), cursor);
	// The ground at (10, 4, 0) raised by 1: the same line as his raised ground at (10, 5, 0)
	const auto towards = hand_feel::FeelDirection(camera, cursor, glm::vec3(10.0f, 4.0f, 0.0f), 1.0f);
	EXPECT_NEAR(towards.x, glm::normalize(glm::vec3(10.0f, -5.0f, 0.0f)).x, 1e-6f);
	EXPECT_NEAR(glm::length(towards), 1.0f, 1e-6f);
}

TEST(HandFeel, TheHandStandsOffTheFaceLeaningToTheCamera)
{
	const glm::vec3 camera(0.0f, 0.0f, 0.0f);
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// A wall facing the camera, met 10 ahead, its normal turned along the line
	const auto rest =
	    hand_feel::RestOnModel(camera, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, true, std::nullopt);
	EXPECT_EQ(rest.point, glm::vec3(0.0f, 0.0f, -10.0f));
	ASSERT_TRUE(rest.up.has_value());
	// A quarter towards the camera, off the face, and half up
	EXPECT_NEAR(rest.up->x, 0.0f, 1e-6f);
	EXPECT_NEAR(rest.up->y, 0.5f, 1e-6f);
	EXPECT_NEAR(rest.up->z, 1.25f, 1e-6f);
	// A model not felt keeps the point but turns the hand to the land
	EXPECT_FALSE(
	    hand_feel::RestOnModel(camera, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, false, std::nullopt)
	        .up.has_value());
}

// The face's normal is made a unit first, as the original makes it after its mesh test
TEST(HandFeel, TheFacesNormalIsMadeAUnitFirst)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	const auto rest =
	    hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -2.0f}, true, std::nullopt);
	ASSERT_TRUE(rest.up.has_value());
	EXPECT_NEAR(rest.up->z, 1.25f, 1e-6f);
}

TEST(HandFeel, AFaceNearerThanAUnitPutsTheHandAUnitAlongTheLine)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	const auto rest =
	    hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -0.5f}, {0.0f, 0.0f, -1.0f}, true, std::nullopt);
	EXPECT_EQ(rest.point, glm::vec3(0.0f, 0.0f, -1.0f));
	ASSERT_TRUE(rest.up.has_value());
	EXPECT_NEAR(rest.up->z, 0.25f + 1.0f, 1e-6f);
}

// Holding something, a face nearer than a unit puts the hand a unit along the line of sight, not the raised direction
// felt, as the original
TEST(HandFeel, AFaceNearerThanAUnitWhileHoldingIsAUnitAlongTheLineOfSight)
{
	const glm::vec3 cursor(0.0f, 0.0f, -1.0f);
	const glm::vec3 felt(0.0f, -0.6f, -0.8f);
	const auto rest = hand_feel::RestOnModel({}, cursor, felt, {0.0f, -0.3f, -0.4f}, {0.0f, 0.0f, 1.0f}, true,
	                                         hand_feel::Holding {.held = false});
	EXPECT_NEAR(rest.point.x, 0.0f, 1e-6f);
	EXPECT_NEAR(rest.point.y, 0.0f, 1e-6f);
	EXPECT_NEAR(rest.point.z, -1.0f, 1e-6f);
	// The hand faces back along the line of sight
	ASSERT_TRUE(rest.up.has_value());
	EXPECT_NEAR(rest.up->z, 0.25f - 1.0f, 1e-6f);
}

TEST(HandFeel, HoldingSomethingTheHandIsKeptOutFromTheFace)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// Kept out by half the held thing's radius, and by how deep in a creature's reach the point is
	const auto rest = hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, true,
	                                         hand_feel::Holding {.radius = 2.0f, .creature = std::nullopt});
	EXPECT_NEAR(rest.point.z, -9.0f, 1e-5f);
	const auto atCreature = hand_feel::RestOnModel(
	    {}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -1.0f}, true,
	    hand_feel::Holding {.radius = 2.0f, .creature = hand_feel::CreatureReach {.centre = {0.0f, -11.0f}, .radius = 4.0f}});
	EXPECT_NEAR(atCreature.point.z, -6.0f, 1e-5f);
}

TEST(HandFeel, TuggingTheHandFeelsAlongTheLineOfSightButIsNotKeptOut)
{
	const glm::vec3 cursor(0.0f, 0.0f, -1.0f);
	const glm::vec3 felt = glm::normalize(glm::vec3(0.0f, -1.0f, -1.0f));
	// Met 10 along the felt direction: the point is 10 along the line of sight, with nothing held to keep out
	const auto rest = hand_feel::RestOnModel({}, cursor, felt, felt * 10.0f, {0.0f, 0.0f, 1.0f}, true,
	                                         hand_feel::Holding {.radius = 2.0f, .held = false});
	EXPECT_NEAR(rest.point.y, 0.0f, 1e-5f);
	EXPECT_NEAR(rest.point.z, -10.0f, 1e-5f);
}

TEST(HandFeel, AThingWithNoTrianglesIsMetWhereThePickMetIt)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// The pick's own distance places the hand, whatever point is given
	const auto rest =
	    hand_feel::RestOnModel({}, direction, direction, {5.0f, 5.0f, 5.0f}, {0.0f, 0.0f, 1.0f}, false, std::nullopt, 12.0f);
	EXPECT_EQ(rest.point, glm::vec3(0.0f, 0.0f, -12.0f));
}

TEST(HandFeel, AVillagerIsHoveredBeforeAtItsDistanceLessItsRadius)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	// The cursor's point on the near plane, one ahead of the camera
	const auto point = hand_feel::RestAtPosition({}, direction, direction, {0.0f, 0.0f, -20.0f}, 2.0f);
	EXPECT_NEAR(point.z, -18.0f, 1e-5f);
}

// The height correction rises with the direction felt (holding, the raised one), as the original
TEST(HandFeel, AVillagerBelowTheLineIsHoveredBeforeByTheFeltDirectionsRise)
{
	const glm::vec3 nearPoint(0.0f, 0.0f, -1.0f);
	const glm::vec3 felt(0.0f, -0.6f, -0.8f);
	// 13 from the camera, its radius 3: 10 along the line, 5 above the villager, so 10 - 5 x 0.6
	const auto point = hand_feel::RestAtPosition({}, nearPoint, felt, {0.0f, -5.0f, -12.0f}, 3.0f);
	EXPECT_NEAR(point.z, -7.0f, 1e-5f);
}

// Each sum in the original's order: the distance's squares x, z then y, the points along the cursor's line z, y then
// x and scaled by the distance over the root, the felt direction's way y, x then z
TEST(HandFeel, TheRestsSumsAreAddedInTheOriginalsOrders)
{
	const glm::vec3 camera {1203.37f, 87.91f, 2210.13f};
	const glm::vec3 nearPoint {1203.71f, 87.23f, 2210.79f};
	const glm::vec3 ground {1219.53f, 41.27f, 2236.61f};
	const float lift = 1.93f;
	const glm::vec3 way {ground.x - camera.x, (ground.y - camera.y) + lift, ground.z - camera.z};
	const float wayShare = 1.0f / std::sqrt((way.y * way.y + way.x * way.x) + way.z * way.z);
	const glm::vec3 felt {way.x * wayShare, way.y * wayShare, way.z * wayShare};
	const auto feltNow = hand_feel::FeelDirection(camera, felt, ground, lift);
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(feltNow[i]), std::bit_cast<uint32_t>(felt[i]));
	}

	const glm::vec3 villager {1215.17f, 44.83f, 2231.29f};
	const float radius = 1.37f;
	const auto apart = villager - camera;
	const float distance = std::sqrt((apart.x * apart.x + apart.z * apart.z) + apart.y * apart.y) - radius;
	const auto along = [&](float length) {
		const auto line = nearPoint - camera;
		const float share = length / std::sqrt((line.z * line.z + line.y * line.y) + line.x * line.x);
		return glm::vec3 {camera.x + line.x * share, camera.y + line.y * share, camera.z + line.z * share};
	};
	const auto first = along(distance);
	const auto expected = along(((first.y - villager.y) * felt.y) + distance);
	const auto point = hand_feel::RestAtPosition(camera, nearPoint, felt, villager, radius);
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(point[i]), std::bit_cast<uint32_t>(expected[i]));
	}
}

TEST(HandFeel, TheHandTurnsToTheLandOffAFaceOrLowOverAThing)
{
	EXPECT_TRUE(hand_feel::TurnsToLand(false, false, 10.0f, 3.2f));
	EXPECT_FALSE(hand_feel::TurnsToLand(true, true, 10.0f, 3.2f));
	EXPECT_TRUE(hand_feel::TurnsToLand(true, true, 1.0f, 3.2f));
	EXPECT_FALSE(hand_feel::TurnsToLand(true, false, 1.0f, 3.2f));
	// Exactly half its height above the land it keeps the face
	EXPECT_FALSE(hand_feel::TurnsToLand(true, true, 1.6f, 3.2f));
}
