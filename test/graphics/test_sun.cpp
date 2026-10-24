/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <vector>

#include <glm/geometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Graphics/Sun.h"

namespace sun = openblack::graphics::sun;

TEST(Sun, RisesAndSetsWithTheScriptTime)
{
	EXPECT_FALSE(sun::Place(2.0f).has_value());
	EXPECT_FALSE(sun::Place(22.0f).has_value());

	// Coming up a third each hour from 3, on the horizon until 6
	const auto dawn = sun::Place(4.5f);
	ASSERT_TRUE(dawn.has_value());
	EXPECT_FLOAT_EQ(dawn->alpha, 127.5f);
	EXPECT_FLOAT_EQ(dawn->position.y, 0.0f);

	const auto noon = sun::Place(12.0f);
	ASSERT_TRUE(noon.has_value());
	EXPECT_FLOAT_EQ(noon->alpha, 255.0f);
	EXPECT_FLOAT_EQ(noon->position.y, 7500.0f);
	EXPECT_FLOAT_EQ(noon->position.x, -30000.0f);
	EXPECT_FLOAT_EQ(noon->position.z, -30000.0f);

	// Setting as it rose
	EXPECT_FLOAT_EQ(sun::Place(15.0f)->position.y, 3750.0f);
	EXPECT_FLOAT_EQ(sun::Place(19.5f)->alpha, 127.5f);
}

TEST(Sun, GlareEasesTowardsWhatShows)
{
	// A hundredth of the way each millisecond
	EXPECT_FLOAT_EQ(sun::EaseGlare(0.0f, 0, 10), 25.5f);
	// Two hidden samples aim at three fifths
	EXPECT_FLOAT_EQ(sun::EaseGlare(255.0f, 2, 50), 255.0f - (102.0f * 0.5f));
	// Paused, it stays put; a long frame doesn't overshoot past the bounds
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 0, 0), 100.0f);
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 0, 199), 255.0f);
	EXPECT_FLOAT_EQ(sun::EaseGlare(100.0f, 5, 199), 0.0f);
}

TEST(Sun, GlareAlphaIsAWholeNumberThroughTheOvercast)
{
	// G x A x (1 / 255) in float: 255 x 255 gives a little over 255, cut to 255
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 0.0f, false), 255);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 0.0f, true), 255);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 127.5f, 0.0f, true), 127);
	EXPECT_EQ(sun::GlareAlpha(100.0f, 85.0f, 0.0f, false), 33);
	EXPECT_EQ(sun::GlareAlpha(254.9f, 255.0f, 0.0f, false), 254);
	EXPECT_EQ(sun::GlareAlpha(0.0f, 255.0f, 1.0f, true), 0);
	// With the fog setting and an overcast, divided by 8 x overcast + 1 and cut again
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 0.25f, true), 85);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 0.5f, true), 51);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 1.0f, true), 28);
	EXPECT_EQ(sun::GlareAlpha(200.0f, 255.0f, 1.0f, true), 22);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 127.5f, 1.0f, true), 14);
	EXPECT_EQ(sun::GlareAlpha(204.0f, 255.0f, 0.25f, true), 68);
	// The first cut comes before the division: 10.9 becomes 10, and 10 / 1.5 gives 6 where 10.9 / 1.5 would give 7
	EXPECT_EQ(sun::GlareAlpha(10.9f, 255.0f, 0.0f, true), 10);
	EXPECT_EQ(sun::GlareAlpha(10.9f, 255.0f, 0.0625f, true), 6);
	// No division without the fog setting, nor with no overcast or one below 0; none past a full one
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 0.5f, false), 255);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, -0.5f, true), 255);
	EXPECT_EQ(sun::GlareAlpha(255.0f, 255.0f, 2.0f, true), 28);
}

TEST(Sun, GlareLooksAtTheMiddleAndTheFourCorners)
{
	ASSERT_EQ(sun::k_GlareSamples.size(), 5U);
	EXPECT_EQ(sun::k_GlareSamples[0], glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(sun::k_GlareSamples[1], glm::vec2(-500.0f, -500.0f));
	EXPECT_EQ(sun::k_GlareSamples[2], glm::vec2(-500.0f, 500.0f));
	EXPECT_EQ(sun::k_GlareSamples[3], glm::vec2(500.0f, -500.0f));
	EXPECT_EQ(sun::k_GlareSamples[4], glm::vec2(500.0f, 500.0f));

	// Across the world's x and up its y, carried the near clip distance on past the sun, away from the camera
	const glm::vec3 noon(-30000.0f, 7500.0f, -30000.0f);
	const auto points = sun::GlareSamplePoints(noon, glm::vec3(-30000.0f, 7500.0f, 0.0f), 2.0f);
	EXPECT_EQ(points[0], glm::vec3(-30000.0f, 7500.0f, -30002.0f));
	EXPECT_EQ(points[1], glm::vec3(-30500.0f, 7000.0f, -30002.0f));
	EXPECT_EQ(points[2], glm::vec3(-30500.0f, 8000.0f, -30002.0f));
	EXPECT_EQ(points[3], glm::vec3(-29500.0f, 7000.0f, -30002.0f));
	EXPECT_EQ(points[4], glm::vec3(-29500.0f, 8000.0f, -30002.0f));
}

TEST(Sun, GlarePointsStayAboveTheLowestAndFollowTheLineToTheSun)
{
	// On the horizon the middle and the low corners are raised to 10
	const glm::vec3 dawn(-30000.0f, 0.0f, -30000.0f);
	const auto low = sun::GlareSamplePoints(dawn, glm::vec3(-30000.0f, 0.0f, 1000.0f), 3.5f);
	EXPECT_EQ(low[0], glm::vec3(-30000.0f, 10.0f, -30003.5f));
	EXPECT_EQ(low[1], glm::vec3(-30500.0f, 10.0f, -30003.5f));
	EXPECT_EQ(low[2], glm::vec3(-30500.0f, 500.0f, -30003.5f));
	EXPECT_EQ(low[3], glm::vec3(-29500.0f, 10.0f, -30003.5f));
	EXPECT_EQ(low[4], glm::vec3(-29500.0f, 500.0f, -30003.5f));

	// The move is along the unit line from the camera to the sun, the near clip distance long
	const glm::vec3 noon(-30000.0f, 7500.0f, -30000.0f);
	const glm::vec3 camera(1752.0f, 52.0f, 2650.0f);
	const auto points = sun::GlareSamplePoints(noon, camera, 2.0f);
	const glm::vec3 moved = points[0] - noon;
	// (within the float steps of a point 30000 units out)
	EXPECT_NEAR(glm::length(moved), 2.0f, 1e-2f);
	const glm::vec3 along = glm::normalize(noon - camera);
	EXPECT_NEAR(glm::dot(glm::normalize(moved), along), 1.0f, 1e-4f);

	// The move in the original's float order: the squares summed z, y, then x, and each part times the near clip over the
	// length, which the unit line times the near clip misses by a step of the last bit here
	const auto close = sun::GlareSamplePoints(glm::vec3(55.0f, 21.0f, -70.0f), glm::vec3(0.0f), 2.0f);
	EXPECT_EQ(close[0], glm::vec3(56.20263671875f, 21.45918846130371f, -71.53062438964844f));
	EXPECT_EQ(close[3], glm::vec3(556.20263671875f, 10.0f, -71.53062438964844f));

	// A camera at the sun moves nothing
	const auto still = sun::GlareSamplePoints(noon, noon, 2.0f);
	EXPECT_EQ(still[0], noon);
	EXPECT_EQ(still[1], glm::vec3(-30500.0f, 7000.0f, -30000.0f));
}

TEST(Sun, GlareCountsEveryHiddenPoint)
{
	const auto points =
	    sun::GlareSamplePoints(glm::vec3(-30000.0f, 7500.0f, -30000.0f), glm::vec3(-30000.0f, 7500.0f, 0.0f), 2.0f);
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3&) { return false; }), 0);
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3&) { return true; }), 5);
	// Land in the way of everything east of the sun: the two corners there
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3& p) { return p.x > -30000.0f; }), 2);
	// Land in the way of everything below the sun: the two low corners
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3& p) { return p.y < 7500.0f; }), 2);
	// Land up to the sun's height: the middle and the two low corners
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3& p) { return p.y <= 7500.0f; }), 3);
	// Land in the way of the low corner east of the sun alone
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t, const glm::vec3& p) { return p.x > -30000.0f && p.y < 7500.0f; }), 1);

	// The eased glare aims a fifth lower for each: two hidden aim at three fifths of 255
	EXPECT_FLOAT_EQ(
	    sun::EaseGlare(255.0f, sun::HiddenSamples(points, [](size_t, const glm::vec3& p) { return p.x > -30000.0f; }), 100),
	    153.0f);
}

TEST(Sun, GlareTellsEachTestWhichPointItIs)
{
	const auto points =
	    sun::GlareSamplePoints(glm::vec3(-30000.0f, 7500.0f, -30000.0f), glm::vec3(-30000.0f, 7500.0f, 0.0f), 2.0f);
	// Something drawn in front of the middle and the last corner
	EXPECT_EQ(sun::HiddenSamples(points, [](size_t i, const glm::vec3&) { return i == 0 || i == 4; }), 2);
	std::vector<size_t> seen;
	EXPECT_EQ(sun::HiddenSamples(points,
	                             [&seen](size_t i, const glm::vec3&) {
		                             seen.push_back(i);
		                             return false;
	                             }),
	          0);
	EXPECT_EQ(seen, (std::vector<size_t> {0, 1, 2, 3, 4}));
}

TEST(Sun, GlareSamplePixelIsCutTowardsZeroAndOnTheScreen)
{
	const glm::vec2 screen(640.0f, 480.0f);
	// The middle of the screen, in front of the near clip
	EXPECT_EQ(sun::SamplePixel({0.0f, 0.0f, 0.5f, 10.0f}, 1.0f, screen), glm::ivec2(320, 240));
	// Up is towards the top row; the screen's coordinates are cut towards 0, so a hair left of the edge is still on it
	EXPECT_EQ(sun::SamplePixel({-10.0f, 10.0f, 0.5f, 10.0f}, 1.0f, screen), glm::ivec2(0, 0));
	EXPECT_EQ(sun::SamplePixel({-10.001f, 0.0f, 0.5f, 10.0f}, 1.0f, screen), glm::ivec2(0, 240));
	EXPECT_EQ(sun::SamplePixel({9.99f, -9.99f, 0.5f, 10.0f}, 1.0f, screen), glm::ivec2(639, 479));
	// Off the screen: the right and bottom edges and beyond the left
	EXPECT_FALSE(sun::SamplePixel({10.0f, 0.0f, 0.5f, 10.0f}, 1.0f, screen).has_value());
	EXPECT_FALSE(sun::SamplePixel({0.0f, -10.0f, 0.5f, 10.0f}, 1.0f, screen).has_value());
	EXPECT_FALSE(sun::SamplePixel({-10.1f, 0.0f, 0.5f, 10.0f}, 1.0f, screen).has_value());
	// Nearer than the near clip, or behind the camera
	EXPECT_FALSE(sun::SamplePixel({0.0f, 0.0f, 0.5f, 0.9f}, 1.0f, screen).has_value());
	EXPECT_FALSE(sun::SamplePixel({0.0f, 0.0f, 0.5f, -10.0f}, 1.0f, screen).has_value());
	EXPECT_TRUE(sun::SamplePixel({0.0f, 0.0f, 0.5f, 1.0f}, 1.0f, screen).has_value());
}

TEST(Sun, GlareIsHiddenByWhatIsDrawnNearerThanAbout122NearClips)
{
	// 1 - near / depth reaches 64999.5 / 65535 at near x 65535 / 535.5
	EXPECT_NEAR(sun::GlareHidingDepth(1.0f), 65535.0f / 535.5f, 1e-2f);
	EXPECT_NEAR(sun::GlareHidingDepth(0.3f), 0.3f * 65535.0f / 535.5f, 1e-2f);
	EXPECT_NEAR(sun::GlareHidingDepth(3.5f), 3.5f * 65535.0f / 535.5f, 5e-2f);
	// At that depth the 16 bits are halfway between 64999 and 65000
	const float depth = sun::GlareHidingDepth(2.0f);
	EXPECT_NEAR((1.0f - 2.0f / depth) * 65535.0f, 64999.5f, 2e-2f);
}
