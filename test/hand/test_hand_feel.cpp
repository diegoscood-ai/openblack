/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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

// Pinned to openblack: the face's normal is taken as the mesh test gives it, not normalised as in his version (1.25
// there for a normal of length 2)
TEST(HandFeel, TheFacesNormalIsTakenAsTheMeshTestGivesIt)
{
	const glm::vec3 direction(0.0f, 0.0f, -1.0f);
	const auto rest =
	    hand_feel::RestOnModel({}, direction, direction, {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, -2.0f}, true, std::nullopt);
	ASSERT_TRUE(rest.up.has_value());
	EXPECT_NEAR(rest.up->z, 2.25f, 1e-6f);
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

// Pinned to openblack: holding something, a face nearer than a unit puts the hand a unit along the raised direction
// felt, where his version and the original take the line of sight, (0, 0, -1) here; a measured change will take it
TEST(HandFeel, AFaceNearerThanAUnitWhileHoldingIsAUnitAlongTheFeltDirection)
{
	const glm::vec3 cursor(0.0f, 0.0f, -1.0f);
	const glm::vec3 felt(0.0f, -0.6f, -0.8f);
	const auto rest = hand_feel::RestOnModel({}, cursor, felt, {0.0f, -0.3f, -0.4f}, {0.0f, 0.0f, 1.0f}, true,
	                                         hand_feel::Holding {.held = false});
	EXPECT_NEAR(rest.point.x, 0.0f, 1e-6f);
	EXPECT_NEAR(rest.point.y, -0.6f, 1e-6f);
	EXPECT_NEAR(rest.point.z, -0.8f, 1e-6f);
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
	const auto point = hand_feel::RestAtPosition({}, direction, direction, {0.0f, 0.0f, -20.0f}, 2.0f);
	EXPECT_NEAR(point.z, -18.0f, 1e-5f);
}

// Pinned to openblack: the height correction rises with the line of sight, where his version and the original take
// the direction felt (holding, the raised one): -7 here; a measured change will take it
TEST(HandFeel, AVillagerBelowTheLineIsHoveredBeforeByTheLineOfSightsRise)
{
	const glm::vec3 cursor(0.0f, 0.0f, -1.0f);
	const glm::vec3 felt(0.0f, -0.6f, -0.8f);
	// 13 from the camera, its radius 3: 10 along the line, 5 above the villager
	const auto point = hand_feel::RestAtPosition({}, cursor, felt, {0.0f, -5.0f, -12.0f}, 3.0f);
	EXPECT_NEAR(point.z, -10.0f, 1e-5f);
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
