/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/ECS/DrawList/ObjectOnScreen.h against the original's on-screen test at an object's Draw: the "not drawn" mark,
// the near test, the eye inside the radius round the origin, the disc against each screen edge, and the Active flag
// after a full pass. Synthetic values only: the identity world-to-clipping matrix, so a point's (X, Y, Z) is the point,
// a 640x480 screen, and values picked so that every product and sum is exact in float32. Also the generic Draw of
// src/ECS/DrawList/ObjectDraw.h: the inputs from a mesh's box under a Transform, and no mesh counting as on screen

#include <cmath>

#include <limits>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/AxisAlignedBoundingBox.h"
#include "ECS/Components/Transform.h"
#include "ECS/DrawList/ObjectDraw.h"
#include "ECS/DrawList/ObjectOnScreen.h"

using openblack::ecs::draw_list::ActiveAfterDraw;
using openblack::ecs::draw_list::DrawOnScreen;
using openblack::ecs::draw_list::OnScreen;
using openblack::ecs::draw_list::OnScreenInputs;
using openblack::ecs::draw_list::OnScreenInputsFrom;
using openblack::ecs::draw_list::OnScreenView;

namespace
{
/// An eye far from every test object, so that only the tests that move the origin reach the camera-inside case
constexpr glm::vec3 k_FarEye {0.0f, 10000.0f, 0.0f};
/// One binary step that is exact on every value used here
constexpr float k_Step = 1.0f / 1024.0f;

/// The identity matrix, the near clip at 1 with a near half width of 1 (a 90 degree field of view), 640x480
OnScreenView StraightView()
{
	OnScreenView view;
	view.eye = k_FarEye;
	view.nearClip = 1.0f;
	view.nearHalfWidth = 1.0f;
	view.screen = {640, 480};
	return view;
}

/// An object whose origin is its box centre
OnScreenInputs Object(const glm::vec3& centre, float radius)
{
	return {.boxCentreWorld = centre, .origin = centre, .radius = radius, .dontDraw = false};
}
} // namespace

TEST(DrawListOnScreen, notDrawnMarkCountsAsOnScreen)
{
	const auto view = StraightView();
	auto behind = Object({0.0f, 0.0f, -100.0f}, 1.0f);
	EXPECT_FALSE(OnScreen(behind, view));
	behind.dontDraw = true;
	EXPECT_TRUE(OnScreen(behind, view));
}

TEST(DrawListOnScreen, nearTestOnDepthPlusRadius)
{
	const auto view = StraightView();
	// 0.75 + 0.25 is exactly the near clip: kept
	EXPECT_TRUE(OnScreen(Object({0.0f, 0.0f, 0.75f}, 0.25f), view));
	// One float below it (1 - 2^-24, exact): off
	EXPECT_FALSE(OnScreen(Object({0.0f, 0.0f, std::nextafter(0.75f, 0.0f)}, 0.25f), view));
}

TEST(DrawListOnScreen, nanDepthIsOffScreen)
{
	const auto view = StraightView();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	EXPECT_FALSE(OnScreen(Object({0.0f, 0.0f, nan}, 1.0f), view));
}

TEST(DrawListOnScreen, nearTestComesBeforeTheEyeInsideTest)
{
	const auto view = StraightView();
	auto object = Object({0.0f, 0.0f, -100.0f}, 2.0f);
	object.origin = view.eye;
	EXPECT_FALSE(OnScreen(object, view));
}

TEST(DrawListOnScreen, eyeInsideTheRadiusRoundTheOriginNotTheCentre)
{
	const auto view = StraightView();
	// The centre is far right of the screen: sx = 32320, the disc's radius 64
	auto object = Object({1000.0f, 0.0f, 10.0f}, 2.0f);
	EXPECT_FALSE(OnScreen(object, view));

	// The eye 1 from the origin, inside r = 2, while the centre stays far from the eye: on the screen
	object.origin = view.eye + glm::vec3 {1.0f, 0.0f, 0.0f};
	EXPECT_TRUE(OnScreen(object, view));

	// Exactly r from the origin: r^2 is not larger, so the disc test decides, and it is off
	object.origin = view.eye + glm::vec3 {2.0f, 0.0f, 0.0f};
	EXPECT_FALSE(OnScreen(object, view));
}

TEST(DrawListOnScreen, nanOriginDistanceGoesOnToTheDiscTest)
{
	const auto view = StraightView();
	const float nan = std::numeric_limits<float>::quiet_NaN();
	auto onScreen = Object({0.0f, 0.0f, 10.0f}, 1.0f);
	onScreen.origin = {nan, 0.0f, 0.0f};
	EXPECT_TRUE(OnScreen(onScreen, view));

	auto offScreen = Object({1000.0f, 0.0f, 10.0f}, 1.0f);
	offScreen.origin = {nan, 0.0f, 0.0f};
	EXPECT_FALSE(OnScreen(offScreen, view));
}

TEST(DrawListOnScreen, leftAndRightEdgesCountAsOnScreen)
{
	const auto view = StraightView();
	// Depth 1, r = 0.25: the disc's radius is 0.25 x 640 x 0.5 = 80
	// sx = (-1.25 + 1) x 320 = -80, so sx + 80 is exactly 0
	EXPECT_TRUE(OnScreen(Object({-1.25f, 0.0f, 1.0f}, 0.25f), view));
	// sx = -80.3125
	EXPECT_FALSE(OnScreen(Object({-1.25f - k_Step, 0.0f, 1.0f}, 0.25f), view));
	// sx = 720, so sx - 80 is exactly 640
	EXPECT_TRUE(OnScreen(Object({1.25f, 0.0f, 1.0f}, 0.25f), view));
	// sx = 720.3125
	EXPECT_FALSE(OnScreen(Object({1.25f + k_Step, 0.0f, 1.0f}, 0.25f), view));
}

TEST(DrawListOnScreen, topAndBottomEdgesCountAsOnScreen)
{
	const auto view = StraightView();
	// Depth 1, r = 0.75: the disc's radius is 240
	// sy = (1 - 2) x 240 = -240, so sy + 240 is exactly 0
	EXPECT_TRUE(OnScreen(Object({0.0f, 2.0f, 1.0f}, 0.75f), view));
	// sy = -240.234375
	EXPECT_FALSE(OnScreen(Object({0.0f, 2.0f + k_Step, 1.0f}, 0.75f), view));
	// sy = 720, so sy - 240 is exactly 480
	EXPECT_TRUE(OnScreen(Object({0.0f, -2.0f, 1.0f}, 0.75f), view));
	// sy = 720.234375
	EXPECT_FALSE(OnScreen(Object({0.0f, -2.0f - k_Step, 1.0f}, 0.75f), view));
}

TEST(DrawListOnScreen, discRadiusScalesWithNearClipOverNearHalfWidth)
{
	// Just past the left edge with the straight view (the disc's radius 80, sx = -80.3125)
	const auto object = Object({-1.25f - k_Step, 0.0f, 1.0f}, 0.25f);
	auto view = StraightView();
	EXPECT_FALSE(OnScreen(object, view));

	// Half the near half width doubles the disc's radius to 160: on the screen
	view.nearHalfWidth = 0.5f;
	EXPECT_TRUE(OnScreen(object, view));

	// Halving the near clip as well brings it back to 80: off again
	view.nearClip = 0.5f;
	EXPECT_FALSE(OnScreen(object, view));
}

TEST(DrawListOnScreen, nanRightEdgeStaysOnScreen)
{
	const auto view = StraightView();
	const float infinity = std::numeric_limits<float>::infinity();
	// An infinite radius and an origin infinitely far: r^2 is not larger than the distance, so the disc test runs.
	// sx and the disc's radius are both infinite: sx + rr is infinite, sx - rr is NaN, which does not fail the right
	// edge, and the other two edges pass
	auto object = Object({1.0e38f, 0.0f, 1.0f}, infinity);
	object.origin = {1.0e30f, 0.0f, 0.0f};
	EXPECT_TRUE(OnScreen(object, view));
}

TEST(DrawListOnScreen, activeAfterDrawFallsBackToHumanOrComplex)
{
	EXPECT_TRUE(ActiveAfterDraw(true, false, false));
	EXPECT_TRUE(ActiveAfterDraw(false, true, false));
	EXPECT_TRUE(ActiveAfterDraw(false, false, true));
	EXPECT_TRUE(ActiveAfterDraw(true, true, true));
	EXPECT_FALSE(ActiveAfterDraw(false, false, false));
}

TEST(DrawListObjectDraw, inputsAreTheBoxUnderTheTransform)
{
	const openblack::ecs::components::Transform transform {
	    .position = {10.0f, 0.0f, 20.0f},
	    .rotation = glm::mat3(1.0f),
	    .scale = {1.0f, 2.0f, 0.5f},
	};
	const openblack::AxisAlignedBoundingBox box {.minima = {-1.0f, 0.0f, -1.0f}, .maxima = {1.0f, 4.0f, 1.0f}};
	const auto inputs = OnScreenInputsFrom(transform, box, false);
	// the box's centre (0, 2, 0) scaled and moved to the object
	EXPECT_FLOAT_EQ(inputs.boxCentreWorld.x, 10.0f);
	EXPECT_FLOAT_EQ(inputs.boxCentreWorld.y, 4.0f);
	EXPECT_FLOAT_EQ(inputs.boxCentreWorld.z, 20.0f);
	EXPECT_EQ(inputs.origin, transform.position);
	// half the box's diagonal, |(2, 4, 2)| / 2, times the largest scale
	EXPECT_FLOAT_EQ(inputs.radius, std::sqrt(24.0f) * 0.5f * 2.0f);
	EXPECT_FALSE(inputs.dontDraw);
	EXPECT_TRUE(OnScreenInputsFrom(transform, box, true).dontDraw);
}

TEST(DrawListObjectDraw, noMeshCountsAsOnScreen)
{
	const auto view = StraightView();
	EXPECT_TRUE(DrawOnScreen(std::nullopt, view));
	auto behind = Object({0.0f, 0.0f, -100.0f}, 1.0f);
	EXPECT_FALSE(DrawOnScreen(behind, view));
	behind.dontDraw = true;
	EXPECT_TRUE(DrawOnScreen(behind, view));
	EXPECT_TRUE(DrawOnScreen(Object({0.0f, 0.0f, 10.0f}, 1.0f), view));
}
