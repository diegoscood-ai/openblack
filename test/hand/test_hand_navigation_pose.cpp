/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <limits>

#include <gtest/gtest.h>

#include "3D/HandNavigationPose.h"

using namespace openblack;
using namespace openblack::hand_navigation_pose;
namespace tricon = camera_drag::tricon;
namespace feature = camera_drag::feature;

TEST(HandNavigationPose, GrippingTheLandShowsTheGrip)
{
	EXPECT_EQ(WhileDragging(true, tricon::k_Rotate), Pose::Grip);
	EXPECT_EQ(WhileDragging(true, 0), Pose::Grip);
}

TEST(HandNavigationPose, DraggingByTheEdgeShowsTurningTiltingOrZooming)
{
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileDragging(false, tricon::k_Pitch), Pose::Pitch);
	EXPECT_EQ(WhileDragging(false, tricon::k_Zoom | tricon::k_Pitch), Pose::Zoom);
	EXPECT_EQ(WhileDragging(false, 0), Pose::Idle);
	// Without the camera help's turning, there is no turning pose, and tilting is not tried instead
	EXPECT_EQ(WhileDragging(false, tricon::k_Rotate | tricon::k_Pitch, feature::k_Pitch), Pose::Idle);
	// Without zooming, the zoom hint shows the ordinary pose, even with the tilt hint
	EXPECT_EQ(WhileDragging(false, tricon::k_Zoom | tricon::k_Pitch, feature::k_Pitch), Pose::Idle);
	EXPECT_EQ(WhileDragging(false, tricon::k_Pitch, feature::k_Rotate), Pose::Idle);
}

TEST(HandNavigationPose, HoveringAtTheEdgeOffersTurning)
{
	EXPECT_EQ(WhileHovering(tricon::k_Idle), std::nullopt);
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Turning), Pose::Rotate);
	// Turning wins over tilting at the very bottom and the top
	EXPECT_EQ(WhileHovering(tricon::k_Idle | tricon::k_Rotate | tricon::k_Pitch), Pose::Rotate);
	EXPECT_EQ(WhileHovering(tricon::k_Pitch), Pose::Pitch);
	// The zoom hint offers nothing while hovering
	EXPECT_EQ(WhileHovering(tricon::k_Zoom), std::nullopt);
	// Without turning, the rotate hint offers nothing, not even the tilt beside it
	EXPECT_EQ(WhileHovering(tricon::k_Rotate | tricon::k_Pitch, feature::k_Pitch), std::nullopt);
}

TEST(HandNavigationPose, EachPoseHasItsAnimation)
{
	EXPECT_EQ(ClipName(Pose::Idle), "Cwiggle");
	EXPECT_EQ(ClipName(Pose::Grip), "Cgrip");
	EXPECT_EQ(ClipName(Pose::Rotate), "Crotate");
	EXPECT_EQ(ClipName(Pose::Pitch), "Cpitch");
	EXPECT_EQ(ClipName(Pose::Zoom), "Czoom");
}

TEST(HandNavigationPose, StandsUpTowardsTheFocusByThreeTenths)
{
	const auto up = UpTowardsFocus({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	// (10, 3, 0) made a unit vector
	const float inverse = 1.0f / std::sqrt(109.0f);
	EXPECT_EQ(up.x, 10.0f * inverse);
	EXPECT_EQ(up.y, 3.0f * inverse);
	EXPECT_EQ(up.z, 0.0f);
}

TEST(HandNavigationPose, TheLengthsSumYThenXThenZ)
{
	const glm::vec3 from(1.5f, -2.25f, 7.0f);
	const glm::vec3 focus(-3.125f, 4.5f, 0.375f);
	const float x = focus.x - from.x;
	const float y = focus.y - from.y;
	const float z = focus.z - from.z;
	const float xx = x * x;
	const float zz = z * z;
	const float raised = std::sqrt((y * y + xx) + zz) * 0.3f + y;
	const float inverse = 1.0f / std::sqrt((raised * raised + xx) + zz);
	const auto up = UpTowardsFocus(from, focus);
	EXPECT_EQ(up.x, x * inverse);
	EXPECT_EQ(up.y, raised * inverse);
	EXPECT_EQ(up.z, z * inverse);
}

TEST(HandNavigationPose, AtTheFocusTheUpIsLeftAsItIs)
{
	EXPECT_EQ(UpTowardsFocus({1.0f, 2.0f, 3.0f}, {1.0f, 2.0f, 3.0f}), glm::vec3(0.0f));
	const float nan = std::numeric_limits<float>::quiet_NaN();
	const auto up = UpTowardsFocus({0.0f, 0.0f, 0.0f}, {nan, nan, nan});
	EXPECT_TRUE(std::isnan(up.x) && std::isnan(up.y) && std::isnan(up.z));
}

TEST(HandNavigationPose, TheTurningLiftIsAThirdOfTheHandsHeight)
{
	EXPECT_EQ(RotateLift(1.0f), 3.2f * 0.33f);
	EXPECT_EQ(RotateLift(0.5f), 3.2f * 0.5f * 0.33f);
}
