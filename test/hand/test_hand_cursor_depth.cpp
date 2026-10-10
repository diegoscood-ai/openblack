/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/HandCursorDepth.h"

using namespace openblack::ecs::hand_cursor_depth;

TEST(HandCursorDepth, TheDepthIsAlongTheCamerasForward)
{
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(ViewDepth(glm::vec3(0.0f, 0.0f, 10.0f), forward), 10.0f);
	// off the centre, the depth is shorter than the distance
	EXPECT_FLOAT_EQ(ViewDepth(glm::vec3(10.0f, 0.0f, 10.0f), forward), 10.0f);
}

TEST(HandCursorDepth, TheLandCountsItsMarginFurther)
{
	EXPECT_TRUE(LandBeforeObject(10.0f, 12.31f));
	EXPECT_FALSE(LandBeforeObject(10.0f, 12.3f)); // equal is not before
	EXPECT_FALSE(LandBeforeObject(10.0f, 12.0f));
}

TEST(HandCursorDepth, OffTheCentreTheMarginWeighsMoreThanAlongTheRay)
{
	// A ray at 45 degrees: the land at 49.716 and the object at 52.306 along the ray. Along the ray the land (with
	// its margin) would win; in depth in front of the camera it does not
	const glm::vec3 forward(0.0f, 0.0f, 1.0f);
	const glm::vec3 ray = glm::normalize(glm::vec3(1.0f, 0.0f, 1.0f));
	EXPECT_TRUE(49.716f + k_LandMargin < 52.306f);
	EXPECT_FALSE(LandBeforeObject(ViewDepth(ray * 49.716f, forward), ViewDepth(ray * 52.306f, forward)));
	// straight ahead the two agree
	EXPECT_TRUE(LandBeforeObject(ViewDepth(forward * 49.716f, forward), ViewDepth(forward * 52.306f, forward)));
}
