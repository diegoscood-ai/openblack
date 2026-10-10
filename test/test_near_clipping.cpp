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
#include <limits>

#include <gtest/gtest.h>

#include "Camera/CameraPathControl.h"
#include "Camera/NearClipping.h"

using namespace openblack;

TEST(NearClipping, FollowsTheHeightOverTheLand)
{
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(-5.0f, false), 0.3f);
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(0.0f, false), 0.3f);
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(10.0f, false), 1.9f);
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(20.0f, false), 3.5f);
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(500.0f, false), 3.5f);
}

TEST(NearClipping, IsTheOriginalsExpressionToTheBit)
{
	// The original's float steps, each rounded: h x 0.05, x 3.2, + 0.3, from just over the ground to 20 units over it
	for (float height = 0.01f; height <= 20.0f; height += 0.37f)
	{
		EXPECT_EQ(near_clipping::NearPlane(height, false), height * 0.05f * 3.2f + 0.3f) << height;
	}
	// Values the former 0.3 + 0.16 h rounded one or two steps lower
	EXPECT_EQ(std::bit_cast<uint32_t>(near_clipping::NearPlane(0.5f, false)), 0x3EC28F5Du);
	EXPECT_EQ(std::bit_cast<uint32_t>(near_clipping::NearPlane(5.0f, false)), 0x3F8CCCCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(near_clipping::NearPlane(10.0f, false)), 0x3FF33334u);
	EXPECT_EQ(std::bit_cast<uint32_t>(near_clipping::NearPlane(19.99f, false)), 0x405FE5C9u);
}

TEST(NearClipping, TwentyUnitsOverTheLandIsStillTheExpression)
{
	// Only above 20 is it the highest: at 20 the expression gives the same 3.5
	EXPECT_EQ(near_clipping::NearPlane(20.0f, false), 20.0f * 0.05f * 3.2f + 0.3f);
	EXPECT_EQ(near_clipping::NearPlane(std::nextafter(20.0f, 21.0f), false), 3.5f);
	// Not above the ground, or no height at all, is the lowest
	EXPECT_EQ(near_clipping::NearPlane(-0.0f, false), 0.3f);
	EXPECT_EQ(near_clipping::NearPlane(std::numeric_limits<float>::quiet_NaN(), false), 0.3f);
}

TEST(CameraPathControl, AMovementKeyTakesTheCameraBackOnlyWhenItWouldMoveIt)
{
	// 400 units a second: 2 ms is 0.8 of a unit, 3 ms 1.2
	EXPECT_FALSE(camera_path::KeyStepMoves(0));
	EXPECT_FALSE(camera_path::KeyStepMoves(2));
	EXPECT_TRUE(camera_path::KeyStepMoves(3));
	EXPECT_TRUE(camera_path::KeyStepMoves(16));
	// A long frame counts as a tenth of a second
	EXPECT_FLOAT_EQ(camera_path::FrameSeconds(500), 0.1f);
	EXPECT_TRUE(camera_path::KeyStepMoves(500));
}

TEST(CameraPathControl, GrippingTheLandAlwaysTakesTheCameraBack)
{
	EXPECT_TRUE(camera_path::TakesCameraBack(false, 0, true));
	EXPECT_TRUE(camera_path::TakesCameraBack(true, 16, false));
	EXPECT_FALSE(camera_path::TakesCameraBack(true, 2, false));
	EXPECT_FALSE(camera_path::TakesCameraBack(false, 16, false));
}

TEST(NearClipping, ScriptsCanClipClose)
{
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(500.0f, true), 0.1f);
	EXPECT_FLOAT_EQ(near_clipping::NearPlane(0.0f, true), 0.1f);
}
