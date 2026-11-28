/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cloud cases on our Clouds: the layout from a fake stream, the drift along the track, the fade at its
// ends, the turn into the world and the colour by the sky's alignment.

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/Clouds.h"

using openblack::Clouds;

namespace
{
/// A cloud at a place on the track
Clouds::Cloud At(const glm::vec3& local, bool pinned = false)
{
	return {.local = local, .size = 0.0f, .k = 0.0f, .pinned = pinned};
}
} // namespace

TEST(Clouds, LayoutPinsTwoAtTheEnds)
{
	// A fake random picks the low end every time
	const auto layout = Clouds::Layout([](float low, float /*high*/) { return low; });
	ASSERT_EQ(layout.size(), 70u);
	EXPECT_TRUE(layout[0].pinned);
	EXPECT_EQ(layout[0].local, glm::vec3(8000.0f, 500.0f, 0.0f));
	EXPECT_EQ(layout[1].local, glm::vec3(-8000.0f, 500.0f, 0.0f));
	EXPECT_FLOAT_EQ(layout[1].size, 300.0f);
	EXPECT_FALSE(layout[2].pinned);
	EXPECT_EQ(layout[2].local, glm::vec3(-8000.0f, 300.0f, -5000.0f));
	EXPECT_FLOAT_EQ(layout[2].size, 13.0f);
	EXPECT_FLOAT_EQ(layout[2].k, 2.5f);
}

TEST(Clouds, DriftAlongTheTrack)
{
	// 70 units a second
	EXPECT_FLOAT_EQ(Clouds::Move({0.0f, 400.0f, 10.0f}, false, 1000.0f).x, 70.0f);
	EXPECT_EQ(Clouds::Move({0.0f, 400.0f, 10.0f}, true, 1000.0f), glm::vec3(0.0f, 400.0f, 10.0f));
	// Past the end, back to the start
	EXPECT_NEAR(Clouds::Move({7990.0f, 400.0f, 0.0f}, false, 1000.0f).x, -7940.0f, 1e-2f);
}

TEST(Clouds, FadeAtTheTracksEnds)
{
	EXPECT_EQ(Clouds::EdgeAlpha(At({0.0f, 0.0f, 0.0f})), 255);
	EXPECT_EQ(Clouds::EdgeAlpha(At({7000.0f, 0.0f, 0.0f})), 128);
	EXPECT_EQ(Clouds::EdgeAlpha(At({-8000.0f, 0.0f, 0.0f})), 0);
	EXPECT_EQ(Clouds::EdgeAlpha(At({8000.0f, 0.0f, 0.0f}, true)), 192);
}

TEST(Clouds, TurnWithTheWindAboutTheMiddle)
{
	const auto middle = Clouds::WorldPosition(At({0.0f, 400.0f, 0.0f}));
	EXPECT_EQ(middle, glm::vec3(1280.0f, 400.0f, 1280.0f));
	const auto along = Clouds::WorldPosition(At({1000.0f, 0.0f, 0.0f}));
	EXPECT_NEAR(along.x, 1280.0f - 707.10678f, 1e-2f);
	EXPECT_NEAR(along.z, 1280.0f + 707.10678f, 1e-2f);
}

TEST(Clouds, ColourFollowsTheSkysAlignment)
{
	// Our colour takes the light table's last entry as 0xAARRGGBB; only its colour is used, as his full light
	// Good: clear white, drawn a little towards 35
	EXPECT_EQ(Clouds::Colour(1.0f, 0xFFFFFF), 0x00DBDBDBu);
	// Neutral: half opaque
	EXPECT_EQ(Clouds::Colour(0.0f, 0xFFFFFF) >> 24, 0xC8u);
	// Evil: opaque, dark orange
	EXPECT_EQ(Clouds::Colour(-1.0f, 0xFFFFFF) >> 24, 0xFFu);
	// In no light, only the pull towards 35 is left
	EXPECT_EQ(Clouds::Colour(1.0f, 0x000000) & 0xFFFFFFu, 0x00232323u);
}
