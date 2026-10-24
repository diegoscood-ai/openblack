/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's model scale, and the mirror of raffclar's right-handed hand option

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/HandSystemDetail.h"

using namespace openblack::ecs::systems::hand_detail;

TEST(HandHandedness, LeftHandIsTheMeshAsItIs)
{
	// Bit for bit the scale the hand had before the option
	const float handScale = 0.731f;
	const auto scale = HandModelScale(handScale, false);
	EXPECT_EQ(scale, glm::vec3(3.2f * handScale / 555.294f));
}

TEST(HandHandedness, RightHandMirrorsOnlyTheXAxis)
{
	const float handScale = 1.0f;
	const auto left = HandModelScale(handScale, false);
	const auto right = HandModelScale(handScale, true);
	EXPECT_EQ(right.x, -left.x);
	EXPECT_EQ(right.y, left.y);
	EXPECT_EQ(right.z, left.z);
	// The hand stays 3.2 units long whichever hand it is
	EXPECT_NEAR(right.y * 555.294f, 3.2f, 1e-5f);
}

TEST(HandHandedness, ScaleIsAConstant)
{
	static_assert(HandModelScale(1.0f, true).x < 0.0f);
	static_assert(HandModelScale(1.0f, false).x > 0.0f);
}
