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

#include <array>
#include <bit>
#include <numbers>

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

// The matrix is built from the heading's angle, each step rounded to a float as the original's, and the up is used as
// it is given: a slightly short, tilted up stays short in the third column.
TEST(HandOrientation, BuildsTheHandsMatrixFromTheHeading)
{
	const glm::vec3 up {0.1f, 0.98f, -0.05f};
	const auto m = HandRotation(1.234f, up);
	EXPECT_EQ(m[0], glm::vec3(-0.330430657f, -0.0144315641f, -0.943719923f));
	EXPECT_EQ(m[1], glm::vec3(0.925567091f, -0.110893533f, -0.322378904f));
	EXPECT_EQ(m[2], -up);
}

TEST(HandOrientation, FacesBackTowardsTheCameraAlongTheHeading)
{
	// heading 0 looks along -z, so the level facing is +z and the side is +z x +y = -x
	const auto m = HandRotation(0.0f, {0.0f, 1.0f, 0.0f});
	EXPECT_NEAR(m[0].x, -1.0f, 1e-6f);
	EXPECT_NEAR(m[0].z, 0.0f, 1e-6f);
	EXPECT_NEAR(m[1].z, -1.0f, 1e-6f);
	// heading -pi faces exactly -z; an up along the facing gives no side, which is left at zero, not replaced
	const auto edge = HandRotation(-std::numbers::pi_v<float>, {0.0f, 0.0f, 1.0f});
	EXPECT_EQ(edge[0], glm::vec3(0.0f));
}

// The up is made a unit vector with z² first, then y², then x², each step rounded to a float: these bits are the
// original's, and summing x² first gives the last bit of each part differently for the second up
TEST(HandOrientation, NormalisesTheUpSummingZThenYThenX)
{
	const auto bits = [](glm::vec3 v) {
		return std::array {std::bit_cast<uint32_t>(v.x), std::bit_cast<uint32_t>(v.y), std::bit_cast<uint32_t>(v.z)};
	};
	EXPECT_EQ(bits(NormaliseUp({0.1f, 0.98f, -0.05f})), (std::array {0x3DCFA1FBu, 0x3F7E59A1u, 0xBD4FA1FBu}));
	EXPECT_EQ(bits(NormaliseUp({-0.02151407115161419f, 0.8369438052177429f, 0.1879330277442932f})),
	          (std::array {0xBCCD661Au, 0x3F79B3AAu, 0x3E604768u}));
}

// An up whose parts all count as zero is left as it is: no fallback to straight up
TEST(HandOrientation, LeavesAZeroUpAsItIs)
{
	EXPECT_EQ(NormaliseUp(glm::vec3(0.0f)), glm::vec3(0.0f));
	const auto negativeZero = NormaliseUp({-0.0f, 0.0f, -0.0f});
	EXPECT_TRUE(std::signbit(negativeZero.x));
	EXPECT_TRUE(std::signbit(negativeZero.z));
}
