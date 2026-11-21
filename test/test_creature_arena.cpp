/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureArena.h"

using namespace openblack;
using namespace openblack::creature_arena;

namespace
{
/// A point in metres as a position in map units
glm::ivec2 Fixed(float x, float z)
{
	return {map_coords::ToFixed(x), map_coords::ToFixed(z)};
}
} // namespace

TEST(CreatureArena, ReuseDistanceGrowsWithSizeAndRunningShare)
{
	// Size 1: (0.875 + 0.25) * 20 = 22.5, times 1.1 and ten times that
	EXPECT_NEAR(ReuseDistance(1.0f, 1.0f), 247.5f, 1e-3f);
	EXPECT_NEAR(ReuseDistance(0.5f, 1.0f), 123.75f, 1e-3f);
	// The running share is kept within 0 and 1
	EXPECT_NEAR(ReuseDistance(2.0f, 1.0f), 247.5f, 1e-3f);
	EXPECT_FLOAT_EQ(ReuseDistance(-1.0f, 1.0f), 0.0f);
	// Size 2: (1.75 + 0.25) * 20 = 40
	EXPECT_NEAR(ReuseDistance(1.0f, 2.0f), 440.0f, 1e-3f);
}

TEST(CreatureArena, NearestArenaWithinTheDistance)
{
	const std::vector<glm::ivec2> arenas {Fixed(100.0f, 0.0f), Fixed(30.0f, 0.0f), Fixed(0.0f, 50.0f)};
	EXPECT_EQ(Nearest(arenas, Fixed(0.0f, 0.0f), 200.0f), 1u);
	// Only those strictly nearer than the distance
	EXPECT_EQ(Nearest(arenas, Fixed(0.0f, 0.0f), 20.0f), std::nullopt);
	EXPECT_EQ(Nearest({}, Fixed(0.0f, 0.0f), 200.0f), std::nullopt);
	// Of two as near, the first in the list
	const std::vector<glm::ivec2> even {Fixed(40.0f, 0.0f), Fixed(-40.0f, 0.0f)};
	EXPECT_EQ(Nearest(even, Fixed(0.0f, 0.0f), 200.0f), 0u);
}

TEST(CreatureArena, SmallerArenasAreTriedDownToSixTenths)
{
	std::vector<float> widths;
	const auto never = [&widths](float width) -> std::optional<glm::vec2> {
		widths.push_back(width);
		return std::nullopt;
	};
	EXPECT_EQ(Place(50.0f, never), std::nullopt);
	// the size wanted, then nine, eight, seven and six tenths of it, each a float a tenth below the last
	ASSERT_EQ(widths.size(), 5u);
	EXPECT_FLOAT_EQ(widths.front(), 100.0f);
	float scale = 1.0f;
	for (const float width : widths)
	{
		const float radius = scale * 50.0f;
		EXPECT_EQ(width, radius + radius);
		scale -= 0.1f;
	}

	// room for a circle of about 60 only: a radius of 50 fits only at six tenths
	const auto room = [](float width) -> std::optional<glm::vec2> {
		return width < 61.0f ? std::optional(glm::vec2(1030.0f, 1030.0f)) : std::nullopt;
	};
	const auto placed = Place(50.0f, room);
	ASSERT_TRUE(placed.has_value());
	EXPECT_NEAR(placed->scale, 0.6f, 1e-5f);
	EXPECT_NEAR(placed->radius, 30.0f, 1e-4f);
	EXPECT_EQ(placed->centre, glm::vec2(1030.0f, 1030.0f));
}
