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

#include "3D/HandOrientation.h"

using namespace openblack::hand_orientation;

namespace
{
void ExpectNear(glm::vec3 actual, glm::vec3 expected)
{
	EXPECT_NEAR(actual.x, expected.x, 1e-5f);
	EXPECT_NEAR(actual.y, expected.y, 1e-5f);
	EXPECT_NEAR(actual.z, expected.z, 1e-5f);
}
} // namespace

TEST(HandOrientation, FacesAlongTheLevelledLineOfSight)
{
	ExpectNear(HeadingAlongRay(glm::normalize(glm::vec3(3.0f, -4.0f, 4.0f)), {0.0f, 0.0f, 1.0f}),
	           glm::normalize(glm::vec3(3.0f, 0.0f, 4.0f)));
}

TEST(HandOrientation, KeepsItsHeadingLookingStraightDown)
{
	const glm::vec3 previous {1.0f, 0.0f, 0.0f};
	ExpectNear(HeadingAlongRay(glm::normalize(glm::vec3(0.005f, -1.0f, -0.008f)), previous), previous);
	// Just past the lean it turns
	ExpectNear(HeadingAlongRay(glm::vec3(0.0f, -0.99f, -0.0101f), previous), {0.0f, 0.0f, -1.0f});
}
