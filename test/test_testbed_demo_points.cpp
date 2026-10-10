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

#include <vector>

#include <glm/trigonometric.hpp>
#include <gtest/gtest.h>

#include "3D/FlatLand.h"
#include "Debug/TestbedDemoPoints.h"
#include "Input/HandDemo.h"

using namespace openblack;

namespace
{
constexpr glm::vec3 k_Eye {0.0f, 10.0f, 0.0f};
/// Looking north (+z) and down at 45 degrees
constexpr glm::vec3 k_Focus {0.0f, 0.0f, 10.0f};
/// A field of view of 90 degrees across, on a square screen: the screen's edges are one unit out at one unit ahead
constexpr testbed_demo::Lens k_Square {.xFovDegrees = 90.0f, .aspect = 1.0f};

hand_demo::Record Move(glm::vec2 mouse, glm::vec3 eye, uint32_t timeMs)
{
	return {.message = 0,
	        .mouse = mouse,
	        .eye = eye,
	        .focus = eye + glm::vec3(10.0f, -10.0f, 0.0f),
	        .trigger = 0,
	        .timeMs = timeMs};
}

hand_demo::Record Button(uint32_t message, uint32_t timeMs)
{
	// A button's own mouse and camera are not the press's
	return {.message = message,
	        .mouse = glm::vec2(0.9f),
	        .eye = glm::vec3(-1.0f),
	        .focus = glm::vec3(-2.0f),
	        .trigger = 0,
	        .timeMs = timeMs};
}

void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1e-3f);
	EXPECT_NEAR(actual.y, expected.y, 1e-3f);
	EXPECT_NEAR(actual.z, expected.z, 1e-3f);
}
} // namespace

TEST(TestbedDemoPoints, APressTakesTheMouseAndCameraOfTheLastMoveBeforeIt)
{
	const std::vector records {
	    Button(1, 0), // no move before it: left out
	    Move({0.1f, 0.2f}, {1.0f, 50.0f, 1.0f}, 16),
	    Move({0.3f, 0.4f}, {2.0f, 60.0f, 2.0f}, 32),
	    Button(1, 48),
	    Button(2, 64),
	    Move({0.5f, 0.6f}, {3.0f, 70.0f, 3.0f}, 80),
	    Button(4, 96), // a release is not a press
	    Button(3, 112),
	};
	const auto presses = testbed_demo::Presses(records);
	ASSERT_EQ(presses.size(), 2u);

	EXPECT_EQ(presses[0].record, 3u);
	EXPECT_EQ(presses[0].message, 1u);
	EXPECT_EQ(presses[0].mouse, glm::vec2(0.3f, 0.4f));
	EXPECT_EQ(presses[0].eye, glm::vec3(2.0f, 60.0f, 2.0f));
	EXPECT_EQ(presses[0].focus, glm::vec3(12.0f, 50.0f, 2.0f));
	EXPECT_EQ(presses[0].timeMs, 48u);

	EXPECT_EQ(presses[1].record, 7u);
	EXPECT_EQ(presses[1].message, 3u);
	EXPECT_EQ(presses[1].mouse, glm::vec2(0.5f, 0.6f));
	EXPECT_EQ(presses[1].eye, glm::vec3(3.0f, 70.0f, 3.0f));

	const auto second = testbed_demo::NthPress(records, 1);
	ASSERT_TRUE(second.has_value());
	EXPECT_EQ(second->record, 7u);
	EXPECT_FALSE(testbed_demo::NthPress(records, 2).has_value());
	EXPECT_FALSE(testbed_demo::NthPress({}, 0).has_value());
}

TEST(TestbedDemoPoints, TheScreensMiddleMeetsThePlaneWhereTheCameraLooks)
{
	const auto ground = testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 0.5f}, k_Square, 0.0f);
	ASSERT_TRUE(ground.has_value());
	ExpectNear(*ground, {0.0f, 0.0f, 10.0f});
	const auto raised = testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 0.5f}, k_Square, 5.0f);
	ASSERT_TRUE(raised.has_value());
	ExpectNear(*raised, {0.0f, 5.0f, 5.0f});
}

TEST(TestbedDemoPoints, TheScreensEdgesAreAcrossTheFieldOfView)
{
	// The right edge one unit to the right of the forward axis at one unit ahead, along the ray of length sqrt(2)
	const auto right = testbed_demo::PointOnPlane(k_Eye, k_Focus, {1.0f, 0.5f}, k_Square, 0.0f);
	ASSERT_TRUE(right.has_value());
	ExpectNear(*right, {10.0f * std::sqrt(2.0f), 0.0f, 10.0f});
	const auto left = testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.0f, 0.5f}, k_Square, 0.0f);
	ASSERT_TRUE(left.has_value());
	ExpectNear(*left, {-10.0f * std::sqrt(2.0f), 0.0f, 10.0f});
	// The bottom edge looks straight down
	const auto bottom = testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 1.0f}, k_Square, 0.0f);
	ASSERT_TRUE(bottom.has_value());
	ExpectNear(*bottom, {0.0f, 0.0f, 0.0f});

	const auto ray = testbed_demo::RayThroughScreen(k_Eye, k_Focus, {0.5f, 0.5f}, k_Square);
	EXPECT_EQ(ray.origin, k_Eye);
	EXPECT_NEAR(ray.direction.y, -std::sqrt(0.5f), 1e-6f);
	EXPECT_NEAR(ray.direction.z, std::sqrt(0.5f), 1e-6f);
}

TEST(TestbedDemoPoints, AWiderScreenLooksLessFarUpAndDown)
{
	// With twice the aspect the top and bottom edges are half as far out: the bottom edge meets the ground at
	// 10 * tan(45 - atan(0.5)) ahead
	const testbed_demo::Lens wide {.xFovDegrees = 90.0f, .aspect = 2.0f};
	const auto bottom = testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 1.0f}, wide, 0.0f);
	ASSERT_TRUE(bottom.has_value());
	const float angle = glm::radians(45.0f) - std::atan(0.5f);
	ExpectNear(*bottom, {0.0f, 0.0f, 10.0f * std::tan(angle)});
}

TEST(TestbedDemoPoints, MissesWhenTheRayDoesNotComeDownToThePlane)
{
	// The top edge looks level
	EXPECT_FALSE(testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 0.0f}, k_Square, 0.0f).has_value());
	// The eye on or under the plane
	EXPECT_FALSE(testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 0.5f}, k_Square, 10.0f).has_value());
	EXPECT_FALSE(testbed_demo::PointOnPlane(k_Eye, k_Focus, {0.5f, 0.5f}, k_Square, 12.0f).has_value());
	// Looking up
	EXPECT_FALSE(testbed_demo::PointOnPlane(k_Eye, {0.0f, 20.0f, 10.0f}, {0.5f, 0.5f}, k_Square, 0.0f).has_value());
	// Looking straight down the camera has no axes
	EXPECT_FALSE(testbed_demo::PointOnPlane(k_Eye, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f}, k_Square, 0.0f).has_value());
}

TEST(TestbedDemoPoints, APressMeetsThePlaneThroughItsMove)
{
	const testbed_demo::Press press {.record = 1, .message = 1, .mouse = {0.5f, 0.5f}, .eye = k_Eye, .focus = k_Focus};
	const auto point = testbed_demo::PointOnPlane(press, k_Square, 0.0f);
	ASSERT_TRUE(point.has_value());
	ExpectNear(*point, {0.0f, 0.0f, 10.0f});
}

TEST(TestbedDemoPoints, RoundsTheMouseToAPixelAsTheGameDoes)
{
	const auto rounded = testbed_demo::RoundToPixel({0.3337f, 0.25f}, {1000, 800});
	EXPECT_FLOAT_EQ(rounded.x, 334.0f / 1000.0f);
	EXPECT_FLOAT_EQ(rounded.y, 200.0f / 800.0f);
}

TEST(TestbedDemoPoints, PicksAPlaneUnderTheLowestEye)
{
	EXPECT_FLOAT_EQ(testbed_demo::PlaneHeight(flat_land::k_Altitude), 32.0f * 0.67f);

	// Every eye high above the usual plane: the usual plane
	const std::vector high {Move({0.5f, 0.5f}, {0.0f, 100.0f, 0.0f}, 0), Move({0.5f, 0.5f}, {0.0f, 40.0f, 0.0f}, 16)};
	EXPECT_EQ(testbed_demo::PlaneAltitudeUnder(high).value_or(0), flat_land::k_Altitude);

	// A camera close to the ground: (13.9 - 2) / 0.67 is 17.8, so altitude 17, 11.39 units, under the eye by the clearance
	const std::vector low {Move({0.5f, 0.5f}, {0.0f, 50.0f, 0.0f}, 0), Move({0.5f, 0.5f}, {0.0f, 13.9f, 0.0f}, 16)};
	const auto altitude = testbed_demo::PlaneAltitudeUnder(low);
	ASSERT_TRUE(altitude.has_value());
	EXPECT_EQ(*altitude, 17);
	EXPECT_LE(testbed_demo::PlaneHeight(*altitude), 13.9f - testbed_demo::k_EyeClearance);
	EXPECT_EQ(testbed_demo::PlaneAltitudeUnder(low, 0.0f).value_or(0), 20);

	// No plane above sea level fits under an eye at the water, nor under no records
	const std::vector tooLow {Move({0.5f, 0.5f}, {0.0f, 4.0f, 0.0f}, 0)};
	EXPECT_FALSE(testbed_demo::PlaneAltitudeUnder(tooLow).has_value());
	EXPECT_FALSE(testbed_demo::PlaneAltitudeUnder({}).has_value());
}
