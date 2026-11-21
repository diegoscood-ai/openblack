/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a line meets a creature's skin (Creature/CreatureSkinRay.h), on parts made up here: from either side, the
// nearest ahead of the line's start, not on an edge, not along the plane, and the spot found again as the parts move

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureSkinRay.h"

using namespace openblack;
namespace ray = openblack::creature_skin_ray;

namespace
{
/// A square of side 2 standing across z at a depth, as two triangles
cave_copy_pick::Part Square(float z)
{
	return {.corners = {{0.0f, 0.0f, z}, {2.0f, 0.0f, z}, {2.0f, 2.0f, z}, {0.0f, 2.0f, z}},
	        .uvs = {},
	        .indices = {0, 1, 2, 0, 2, 3},
	        .primitives = {{.first = 0, .count = 6, .twoSided = false}}};
}
} // namespace

TEST(CreatureSkinRay, TheNearestTriangleAheadIsMetFromEitherSide)
{
	const std::vector parts {Square(5.0f), Square(3.0f)};
	const glm::vec3 start(1.5f, 0.5f, 0.0f);
	const auto hit = ray::Nearest(parts, start, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(hit.has_value());
	EXPECT_EQ(hit->spot.part, 1u);
	EXPECT_FLOAT_EQ(hit->distance, 3.0f);
	EXPECT_FLOAT_EQ(hit->point.z, 3.0f);
	// from behind it is met all the same
	const auto back = ray::Nearest(parts, {1.5f, 0.5f, 10.0f}, {0.0f, 0.0f, -1.0f});
	ASSERT_TRUE(back.has_value());
	EXPECT_EQ(back->spot.part, 0u);
	// nothing behind the line's start, nothing along the plane
	EXPECT_FALSE(ray::Nearest(parts, {1.5f, 0.5f, 10.0f}, {0.0f, 0.0f, 1.0f}).has_value());
	EXPECT_FALSE(ray::Nearest(parts, {1.5f, 0.5f, 3.0f}, {1.0f, 0.0f, 0.0f}).has_value());
	// nor beside it
	EXPECT_FALSE(ray::Nearest(parts, {5.0f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}).has_value());
}

// The spot keeps which primitive of its part the triangle met is in
TEST(CreatureSkinRay, TheSpotKeepsThePrimitiveOfItsTriangle)
{
	auto square = Square(3.0f);
	square.primitives = {{.first = 0, .count = 3, .twoSided = false}, {.first = 3, .count = 3, .twoSided = false}};
	const std::vector parts {square};
	// below the diagonal: the first triangle, the first primitive
	const auto low = ray::Nearest(parts, {1.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(low.has_value());
	EXPECT_EQ(low->spot.primitive, 0u);
	// above it: the second
	const auto high = ray::Nearest(parts, {0.5f, 1.5f, 0.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(high.has_value());
	EXPECT_EQ(high->spot.primitive, 1u);
	EXPECT_EQ(high->spot.corners, (std::array<uint16_t, 3> {0, 2, 3}));
}

TEST(CreatureSkinRay, TheSpotIsFoundAgainAsThePartsMove)
{
	std::vector parts {Square(3.0f)};
	const auto hit = ray::Nearest(parts, {1.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(hit.has_value());
	const auto here = ray::PointOf(parts, hit->spot);
	ASSERT_TRUE(here.has_value());
	EXPECT_NEAR(here->x, 1.5f, 1e-5f);
	EXPECT_NEAR(here->y, 0.5f, 1e-5f);
	// the body moves on by one along x: the spot moves with it
	for (auto& corner : parts.front().corners)
	{
		corner.x += 1.0f;
	}
	const auto moved = ray::PointOf(parts, hit->spot);
	ASSERT_TRUE(moved.has_value());
	EXPECT_NEAR(moved->x, 2.5f, 1e-5f);
	// a spot on a part that is gone is found nowhere
	EXPECT_FALSE(ray::PointOf({}, hit->spot).has_value());
}

// The game works out where on a triangle the line met it through its edges' x: through the second edge's, or the
// first's when the second has almost none; a triangle with almost no x along either edge is not counted
TEST(CreatureSkinRay, ATriangleIsWorkedOutThroughItsFirstEdgeWhenItsSecondHasNoXAndNotCountedWithNeither)
{
	const cave_copy_pick::Part upright {.corners = {{0.0f, 0.0f, 3.0f}, {2.0f, 0.0f, 3.0f}, {0.0f, 2.0f, 3.0f}},
	                                    .uvs = {},
	                                    .indices = {0, 1, 2},
	                                    .primitives = {{.first = 0, .count = 3, .twoSided = false}}};
	const auto hit = ray::Nearest(std::vector {upright}, {0.5f, 0.25f, 0.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(hit.has_value());
	// a quarter of the way along the first edge (2 across) and an eighth along the second (2 up)
	EXPECT_FLOAT_EQ(hit->spot.u, 0.25f);
	EXPECT_FLOAT_EQ(hit->spot.v, 0.125f);
	// a triangle seen edge on across x: neither edge has any x, so where on it can't be worked out
	const cave_copy_pick::Part across {.corners = {{0.0f, 0.0f, 3.0f}, {0.0f, 2.0f, 3.0f}, {0.0f, 0.0f, 5.0f}},
	                                   .uvs = {},
	                                   .indices = {0, 1, 2},
	                                   .primitives = {{.first = 0, .count = 3, .twoSided = false}}};
	EXPECT_FALSE(ray::Nearest(std::vector {across}, {-1.0f, 0.5f, 3.5f}, {1.0f, 0.0f, 0.0f}).has_value());
}

// The spot on a triangle, from its two edges, in the game's order of operations
TEST(CreatureSkinRay, TheSpotIsWorkedOutThroughTheSecondEdgesXFirst)
{
	// both edges have x: the second's is used
	const auto both = ray::SpotOn({2.0f, 1.0f, 0.0f}, {1.0f, 2.0f, 0.0f}, {1.5f, 1.5f, 0.0f});
	ASSERT_TRUE(both.has_value());
	EXPECT_FLOAT_EQ(both->first, 0.5f);
	EXPECT_FLOAT_EQ(both->second, 0.5f);
	// the second edge has none: the first's
	const auto first = ray::SpotOn({2.0f, 0.0f, 0.0f}, {0.004f, 2.0f, 0.0f}, {1.0f, 1.0f, 0.0f});
	ASSERT_TRUE(first.has_value());
	EXPECT_FLOAT_EQ(first->second, 1.0f / (2.0f - ((0.004f * 0.0f) / 2.0f)));
	EXPECT_FLOAT_EQ(first->first, (1.0f - (first->second * 0.004f)) / 2.0f);
	// the edges too alike, or neither with any x
	EXPECT_FALSE(ray::SpotOn({2.0f, 2.0f, 0.0f}, {1.0f, 1.0f, 0.0f}, {1.0f, 1.0f, 0.0f}).has_value());
	EXPECT_FALSE(ray::SpotOn({0.0f, 2.0f, 0.0f}, {0.0f, 0.0f, 2.0f}, {0.0f, 1.0f, 1.0f}).has_value());
}

// The way out of the skin at a spot is its triangle's first edge crossed with its second: minus the game's inward
// normal (its second edge crossed with its first)
TEST(CreatureSkinRay, TheWayOutIsTheTrianglesFirstEdgeCrossedWithItsSecond)
{
	const std::vector parts {Square(3.0f)};
	const auto hit = ray::Nearest(parts, {1.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f});
	ASSERT_TRUE(hit.has_value());
	const auto out = ray::OutwardOf(parts, hit->spot);
	ASSERT_TRUE(out.has_value());
	// corners (0, 0), (2, 0), (2, 2): (2, 0, 0) x (2, 2, 0) = (0, 0, 4)
	EXPECT_EQ(*out, glm::vec3(0.0f, 0.0f, 4.0f));
	EXPECT_FALSE(ray::OutwardOf(parts, ray::Spot {.part = 5}).has_value());
}
