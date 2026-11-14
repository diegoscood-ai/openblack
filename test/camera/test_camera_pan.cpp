/******************************************************************************
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

#include "Camera/CameraPan.h"

using namespace openblack::camera_pan;

namespace
{
void ExpectNear(glm::vec3 actual, glm::vec3 expected, float epsilon = 1e-3f)
{
	EXPECT_NEAR(actual.x, expected.x, epsilon);
	EXPECT_NEAR(actual.y, expected.y, epsilon);
	EXPECT_NEAR(actual.z, expected.z, epsilon);
}
} // namespace

TEST(CameraPan, OnLevelLandThePlaneIsTheLand)
{
	// As the game recorded it: the camera 3 above level land, gripping the land ahead
	const auto plane = PlaneThrough({1000.0f, 0.0f, 1116.20728f}, {1000.0f, 3.0f, 1120.0f}, 0.0f);
	ExpectNear(plane.normal, {0.0f, 1.0f, 0.0f});
	EXPECT_NEAR(plane.distance, 0.0f, 1e-5f);
}

TEST(CameraPan, OnASlopeThePlaneLeansBackTowardsTheCamera)
{
	// Land gripped up a hill ahead
	const auto plane = PlaneThrough({0.0f, 10.0f, 10.0f}, {0.0f, 50.0f, 0.0f}, 0.0f);
	ExpectNear(plane.normal, glm::normalize(glm::vec3(0.0f, 1.0f, -1.0f)));
	EXPECT_NEAR(plane.distance, glm::dot(plane.normal, glm::vec3(0.0f, 10.0f, 10.0f)), 1e-4f);
	// The ground under the camera counts no higher than the gripped land: from higher ground the plane is level
	const auto lower = PlaneThrough({0.0f, 10.0f, 10.0f}, {0.0f, 50.0f, 0.0f}, 30.0f);
	ExpectNear(lower.normal, {0.0f, 1.0f, 0.0f});
}

TEST(CameraPan, ThePlaneIsMadeWithTheGamesSumsAndTwoUnitSteps)
{
	const glm::vec3 gripped {1033.71f, 12.37f, 2101.93f};
	const glm::vec3 camera {980.13f, 61.9f, 2003.17f};
	const float groundUnderCamera = 8.41f;
	const auto plane = PlaneThrough(gripped, camera, groundUnderCamera);

	// Each step made a unit with its squares added z, y then x, times one over the root; the normal is made one twice
	const auto zyx = [](const glm::vec3& a, const glm::vec3& b) { return (a.z * b.z + a.y * b.y) + a.x * b.x; };
	const auto unit = [&zyx](const glm::vec3& v) { return v * (1.0f / std::sqrt(zyx(v, v))); };
	const auto towards = unit(gripped - glm::vec3(camera.x, groundUnderCamera, camera.z));
	const auto across = unit({towards.z, 0.0f, -towards.x});
	const auto normal = unit(unit(glm::cross(towards, across)));
	EXPECT_EQ(std::bit_cast<uint32_t>(plane.normal.x), std::bit_cast<uint32_t>(normal.x));
	EXPECT_EQ(std::bit_cast<uint32_t>(plane.normal.y), std::bit_cast<uint32_t>(normal.y));
	EXPECT_EQ(std::bit_cast<uint32_t>(plane.normal.z), std::bit_cast<uint32_t>(normal.z));
	EXPECT_EQ(std::bit_cast<uint32_t>(plane.distance), std::bit_cast<uint32_t>(zyx(normal, gripped)));
}

TEST(CameraPan, TheLandUnderTheCursorStaysUnderIt)
{
	const GripPlane level {.normal = {0.0f, 1.0f, 0.0f}, .distance = 0.0f};
	const glm::vec3 origin {0.0f, 10.0f, 0.0f};
	const glm::vec3 focus {0.0f, 0.0f, 10.0f};
	const auto distance = glm::distance(origin, focus);
	// Gripped straight below and ahead; the cursor now looks 5 further on
	const auto atGrip = glm::normalize(glm::vec3(0.0f, -10.0f, 10.0f));
	const auto now = glm::normalize(glm::vec3(0.0f, -10.0f, 15.0f));
	const auto place = Pan(level, origin, focus, distance, now, atGrip, {400, 200}, {400, 300});
	ASSERT_TRUE(place.has_value());
	// The camera comes 5 back, so the land gripped is where the cursor now looks
	ExpectNear(place->origin, {0.0f, 10.0f, -5.0f});
	ExpectNear(place->focus, {0.0f, 0.0f, 5.0f});
}

TEST(CameraPan, TheDragMovesBetweenTheTwoPointsMetFromTheCamera)
{
	const GripPlane plane {.normal = {0.0f, 1.0f, 0.0f}, .distance = 3.7f};
	const glm::vec3 origin {1000.3f, 251.1f, 2000.7f};
	const glm::vec3 focus {1100.9f, 3.7f, 2100.1f};
	const float distance = 180.3f;
	const glm::vec3 now {0.3137f, -0.9013f, 0.2911f};
	const glm::vec3 atGrip {0.3311f, -0.8977f, 0.2789f};
	const auto place = Pan(plane, origin, focus, distance, now, atGrip, {900, 700}, {100, 100});
	ASSERT_TRUE(place.has_value());

	// The normal times the line now, y then z then x; the line at the grip, z then y then x
	const float dotNow = (plane.normal.y * now.y + plane.normal.z * now.z) + plane.normal.x * now.x;
	const float dotGrip = (plane.normal.z * atGrip.z + plane.normal.y * atGrip.y) + plane.normal.x * atGrip.x;
	const float toPlane =
	    plane.distance - ((plane.normal.z * origin.z + plane.normal.y * origin.y) + plane.normal.x * origin.x);
	const float alongNow = toPlane / dotNow;
	const float alongGrip = toPlane / dotGrip;
	// Both points met are from the camera, and the move is their difference
	const glm::vec3 hitNow {now.x * alongNow + origin.x, now.y * alongNow + origin.y, now.z * alongNow + origin.z};
	const glm::vec3 hitGrip {atGrip.x * alongGrip + origin.x, atGrip.y * alongGrip + origin.y, atGrip.z * alongGrip + origin.z};
	const glm::vec3 moved = hitNow - hitGrip;
	const glm::vec3 newFocus = focus - moved;
	const glm::vec3 back = (origin - moved) - newFocus;
	// The way back is made one long with its squares added z, x, then y
	const float share = 1.0f / std::sqrt((back.z * back.z + back.x * back.x) + back.y * back.y);
	const glm::vec3 unit {back.x * share, back.y * share, back.z * share};
	const glm::vec3 newOrigin {unit.x * distance + newFocus.x, unit.y * distance + newFocus.y, unit.z * distance + newFocus.z};
	for (int i = 0; i < 3; ++i)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(place->focus[i]), std::bit_cast<uint32_t>(newFocus[i]));
		EXPECT_EQ(std::bit_cast<uint32_t>(place->origin[i]), std::bit_cast<uint32_t>(newOrigin[i]));
	}
}

TEST(CameraPan, TheMoveIsLimitedByHowFarTheCursorWent)
{
	const GripPlane level {.normal = {0.0f, 1.0f, 0.0f}, .distance = 0.0f};
	const glm::vec3 origin {0.0f, 10.0f, 0.0f};
	const glm::vec3 focus {0.0f, 0.0f, 100.0f};
	const auto distance = glm::distance(origin, focus);
	const auto atGrip = glm::normalize(glm::vec3(0.0f, -10.0f, 10.0f));
	// Nearly along the land: the land under it is far away
	const auto now = glm::normalize(glm::vec3(0.0f, -1.0f, 200.0f));
	// One pixel moved: no more than 50
	const auto place = Pan(level, origin, focus, distance, now, atGrip, {400, 299}, {400, 300});
	ASSERT_TRUE(place.has_value());
	EXPECT_NEAR(glm::distance(place->focus, focus), 50.0f, 1e-2f);
	// A hundred pixels: 0.011 of the distance for each
	const auto further = Pan(level, origin, focus, distance, now, atGrip, {400, 200}, {400, 300});
	ASSERT_TRUE(further.has_value());
	EXPECT_NEAR(glm::distance(further->focus, focus), 100.0f * distance * 0.011f, 1e-2f);
}

TEST(CameraPan, LookingAboveTheLandDragsNothing)
{
	const GripPlane level {.normal = {0.0f, 1.0f, 0.0f}, .distance = 0.0f};
	const glm::vec3 origin {0.0f, 10.0f, 0.0f};
	const auto atGrip = glm::normalize(glm::vec3(0.0f, -10.0f, 10.0f));
	EXPECT_FALSE(
	    Pan(level, origin, {0.0f, 0.0f, 10.0f}, 14.0f, glm::normalize(glm::vec3(0.0f, 1.0f, 10.0f)), atGrip, {0, 0}, {0, 0})
	        .has_value());
	// Along the land
	EXPECT_FALSE(Pan(level, origin, {0.0f, 0.0f, 10.0f}, 14.0f, {0.0f, 0.0f, 1.0f}, atGrip, {0, 0}, {0, 0}).has_value());
}

TEST(CameraPan, StopsThreeShortOfLandInItsWay)
{
	const CameraPlace place {.origin = {0.0f, 10.0f, 100.0f}, .focus = {0.0f, 0.0f, 110.0f}};
	// Land 50 along the way: the camera stops 3 short of it
	const auto stopped = StopShortOfLand(place, {0.0f, 10.0f, 0.0f}, {0.0f, 5.0f, 50.0f});
	ExpectNear(stopped.origin, {0.0f, 10.0f, 47.0f});
	ExpectNear(stopped.focus, {0.0f, 0.0f, 57.0f});
	// Land beyond the move doesn't stop it
	const auto onward = StopShortOfLand(place, {0.0f, 10.0f, 0.0f}, {0.0f, 5.0f, 200.0f});
	ExpectNear(onward.origin, place.origin);
	// Nor does a move of 3 or less across
	const CameraPlace small {.origin = {0.0f, 10.0f, 3.0f}, .focus = {0.0f, 0.0f, 13.0f}};
	ExpectNear(StopShortOfLand(small, {0.0f, 10.0f, 0.0f}, {0.0f, 5.0f, 1.0f}).origin, small.origin);
}

TEST(CameraPan, TheSeaIsMetGoingDownNearTheCamera)
{
	const auto hit = SeaHit({0.0f, 10.0f, 0.0f}, {0.0f, 5.0f, 10.0f}, {0.0f, 10.0f, 0.0f});
	ASSERT_TRUE(hit.has_value());
	ExpectNear(*hit, {0.0f, 0.0f, 20.0f});
	// Going up, or level, never meets it
	EXPECT_FALSE(SeaHit({0.0f, 10.0f, 0.0f}, {0.0f, 15.0f, 10.0f}, {0.0f, 10.0f, 0.0f}).has_value());
	EXPECT_FALSE(SeaHit({0.0f, 10.0f, 0.0f}, {0.0f, 10.0f, 10.0f}, {0.0f, 10.0f, 0.0f}).has_value());
	// Nor more than 7500 across from the camera
	EXPECT_FALSE(SeaHit({0.0f, 10.0f, 0.0f}, {0.0f, 9.99f, 10.0f}, {0.0f, 10.0f, 0.0f}).has_value());
}
