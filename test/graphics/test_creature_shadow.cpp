/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// graphics::CreatureShadow (src/Graphics/CreatureShadow.h): raffclar's test, on the original's numbers that
// graphics::shadow_math carries

#include <cmath>

#include <initializer_list>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/CreatureShadow.h"
#include "Graphics/ShadowMath.h"

using namespace openblack::graphics;

TEST(CreatureShadow, FadesBetween50And80Radii)
{
	EXPECT_EQ(CreatureShadow::Alpha(10.0f, 1.0f), 255);
	EXPECT_EQ(CreatureShadow::Alpha(49.9f, 1.0f), 255);
	EXPECT_EQ(CreatureShadow::Alpha(65.0f, 1.0f), 127);
	EXPECT_EQ(CreatureShadow::Alpha(80.0f, 1.0f), 0);
	// In radii of the creature
	EXPECT_EQ(CreatureShadow::Alpha(650.0f, 10.0f), 127);
	EXPECT_EQ(CreatureShadow::Alpha(10.0f, 0.0f), 0);
}

TEST(CreatureShadow, TheLightIsKeptAtLeast45DegreesUp)
{
	const glm::vec3 centre {100.0f, 10.0f, 100.0f};
	// A low sun far away, 10 degrees up
	const auto low = CreatureShadow::LightPoint(centre + glm::vec3(1000.0f, 176.3f, 0.0f), centre, 2.0f);
	const auto towards = low - centre;
	EXPECT_NEAR(towards.y, 1000.0f, 1e-2f);
	EXPECT_NEAR(towards.x, 1000.0f, 1e-2f);
	// A high one is left as it is
	const auto high = CreatureShadow::LightPoint(centre + glm::vec3(10.0f, 500.0f, 0.0f), centre, 2.0f);
	EXPECT_NEAR(high.y - centre.y, 500.0f, 1e-3f);
}

TEST(CreatureShadow, ANearLightIsPushedOutToThreeRadii)
{
	const glm::vec3 centre {0.0f, 0.0f, 0.0f};
	const auto pushed = CreatureShadow::LightPoint(glm::vec3(1.0f, 0.5f, 0.0f), centre, 2.0f);
	const auto across = std::sqrt((pushed.x * pushed.x) + (pushed.z * pushed.z));
	// Out to 6 units, then raised to 45 degrees
	EXPECT_GT(across, 5.0f);
	EXPECT_NEAR(pushed.y, across, 1e-4f);
	// Straight overhead, it is nudged to a side first
	const auto overhead = CreatureShadow::LightPoint(glm::vec3(0.0f, 1.0f, 0.0f), centre, 2.0f);
	EXPECT_GT(overhead.x, 0.0f);
	EXPECT_GT(overhead.z, 0.0f);
}

// Ours, not raffclar's: the nudge only below a tenth of a unit across the ground. His 0.2 would move this light aside
TEST(CreatureShadow, OnlyALightWithinATenthIsNudged)
{
	const glm::vec3 centre {0.0f, 0.0f, 0.0f};
	const auto light = CreatureShadow::LightPoint(glm::vec3(0.15f, 1.0f, 0.0f), centre, 2.0f);
	EXPECT_FLOAT_EQ(light.z, 0.0f);
	EXPECT_GT(light.x, 0.0f);
	const auto nudged = CreatureShadow::LightPoint(glm::vec3(0.05f, 1.0f, 0.0f), centre, 2.0f);
	EXPECT_GT(nudged.z, 0.0f);
}

// The light the shadow list takes for a creature: the mesh's radius times the scale gives the same light, bit for bit,
// as shadow_math's from the two apart
TEST(CreatureShadow, TheSameLightAsTheShadowMaths)
{
	const glm::vec3 body {1895.97f, 21.5f, 2520.75f};
	const glm::vec3 sun {-500000.0f, 500000.0f, -500000.0f};
	const glm::vec3 close {1897.0f, 22.0f, 2521.0f};
	for (const auto& light : {sun, close})
	{
		for (const float scale : {0.22f, 1.0f, 1.7f})
		{
			const float meshRadius = 7.3f;
			EXPECT_EQ(CreatureShadow::LightPoint(light, body, meshRadius * scale),
			          shadow_math::LightCreature(body, light, meshRadius, scale));
		}
	}
}

TEST(CreatureShadow, HalfRowsAndTheOriginalsTexture)
{
	EXPECT_TRUE(CreatureShadow::k_HalfRows);
	EXPECT_EQ(CreatureShadow::k_Texels, 32);
	EXPECT_EQ(CreatureShadow::k_FadeStart, 50.0f);
	EXPECT_EQ(CreatureShadow::k_FadeEnd, 80.0f);
	EXPECT_EQ(CreatureShadow::k_LightDistance, 3.0f);
}
