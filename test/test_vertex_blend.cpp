/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <vector>

#include <gtest/gtest.h>

#include "3D/VertexBlend.h"

using namespace openblack;
using namespace openblack::vertex_blend;

TEST(VertexBlend, BlendsNameVerticesInTheirPrimitive)
{
	// Two primitives of 3 and 4 vertices, with one and two blends
	const std::array<uint32_t, 2> vertices {3, 4};
	const std::array<uint32_t, 2> counts {1, 2};
	const std::array<Blend, 3> blends {{
	    {.vertex = 0, .towards = 2, .weight = 0.5f},
	    {.vertex = 1, .towards = 3, .weight = 0.25f},
	    {.vertex = 0, .towards = 2, .weight = 0.125f},
	}};
	const auto partners = Partners(vertices, counts, blends);
	ASSERT_EQ(partners.size(), 7u);
	EXPECT_EQ(partners[0].vertex, 2);
	EXPECT_FLOAT_EQ(partners[0].weight, 0.5f);
	EXPECT_FALSE(partners[1].Blended());
	// The second primitive's vertices start after the first's
	EXPECT_EQ(partners[4].vertex, 6);
	EXPECT_FLOAT_EQ(partners[4].weight, 0.25f);
	EXPECT_EQ(partners[3].vertex, 5);
	EXPECT_FLOAT_EQ(partners[3].weight, 0.125f);
	EXPECT_FALSE(partners[6].Blended());
}

TEST(VertexBlend, BlendsOutsideTheirPrimitiveAreLeftOut)
{
	const std::array<uint32_t, 1> vertices {3};
	const std::array<uint32_t, 1> counts {2};
	const std::array<Blend, 2> blends {{
	    {.vertex = 0, .towards = 5, .weight = 0.5f},
	    {.vertex = 1, .towards = 1, .weight = 0.5f},
	}};
	const auto partners = Partners(vertices, counts, blends);
	EXPECT_FALSE(partners[0].Blended());
	EXPECT_FALSE(partners[1].Blended());
}

TEST(VertexBlend, AVertexMovesItsWeightTowardsWhereItsPartnerWasPlaced)
{
	// Each vertex already placed by its own bone
	std::vector<glm::vec3> positions {{0.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {0.0f, 8.0f, 0.0f}};
	std::vector<Partner> partners(3);
	partners[0] = {.vertex = 1, .weight = 0.5f};
	partners[2] = {.vertex = 1, .weight = 0.25f};
	Apply(positions, partners);
	EXPECT_FLOAT_EQ(positions[0].x, 2.0f);
	EXPECT_FLOAT_EQ(positions[1].x, 4.0f);
	EXPECT_FLOAT_EQ(positions[2].x, 1.0f);
	EXPECT_FLOAT_EQ(positions[2].y, 6.0f);
}

TEST(VertexBlend, PartnersMoveTowardsPlacedPositionsNotBlendedOnes)
{
	// Were the blends chained, the second would see the first's result
	std::vector<glm::vec3> positions {{0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {20.0f, 0.0f, 0.0f}};
	std::vector<Partner> partners(3);
	partners[1] = {.vertex = 2, .weight = 0.5f};
	partners[0] = {.vertex = 1, .weight = 0.5f};
	Apply(positions, partners);
	EXPECT_FLOAT_EQ(positions[0].x, 5.0f);
	EXPECT_FLOAT_EQ(positions[1].x, 15.0f);
}

TEST(VertexBlend, WeightsAreKeptAsTheFileHasThem)
{
	// The weights are drawn as they are read: none is rounded, nor held to a range
	const std::array<uint32_t, 1> vertices {4};
	const std::array<uint32_t, 1> counts {3};
	const std::array<Blend, 3> blends {{
	    {.vertex = 0, .towards = 1, .weight = 0.497673f},
	    {.vertex = 2, .towards = 1, .weight = 1.01173783e-14f},
	    {.vertex = 3, .towards = 1, .weight = 0.0f},
	}};
	const auto partners = Partners(vertices, counts, blends);
	EXPECT_EQ(partners[0].weight, 0.497673f);
	EXPECT_EQ(partners[2].weight, 1.01173783e-14f);
	// A blend of no weight still names its partner; it leaves the vertex where it was placed
	EXPECT_TRUE(partners[3].Blended());
	EXPECT_EQ(partners[3].vertex, 1);
}

TEST(VertexBlend, AVertexMovesByTheDifferenceTimesItsWeight)
{
	// placed + (towards - placed) x weight
	const auto moved = Towards({1.0f, -2.0f, 3.0f}, {5.0f, 2.0f, -1.0f}, 0.25f);
	EXPECT_EQ(moved.x, 2.0f);
	EXPECT_EQ(moved.y, -1.0f);
	EXPECT_EQ(moved.z, 2.0f);
	const glm::vec3 placed {0.1f, 0.2f, 0.3f};
	const glm::vec3 towards {0.7f, -0.4f, 1.9f};
	const auto blended = Towards(placed, towards, 0.497673f);
	EXPECT_EQ(blended.x, placed.x + (towards.x - placed.x) * 0.497673f);
	EXPECT_EQ(blended.y, placed.y + (towards.y - placed.y) * 0.497673f);
	EXPECT_EQ(blended.z, placed.z + (towards.z - placed.z) * 0.497673f);
}
