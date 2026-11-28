/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <bit>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/PartialBuildCap.h"

using namespace openblack::graphics::partial_build_cap;

namespace
{
// A wall one unit tall facing +x, as two triangles, its normals out of the building
constexpr std::array k_Positions {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f),
                                  glm::vec3(1.0f, 1.0f, 0.0f)};
constexpr std::array k_Uvs {glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(1.0f, 1.0f), glm::vec2(0.0f, 1.0f)};
constexpr std::array k_Normals {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(1.0f, 0.0f, 0.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f)};
constexpr std::array<uint16_t, 6> k_Indices {0, 1, 2, 0, 2, 3};
} // namespace

TEST(PartialBuildCap, EachCutTriangleJoinsItsOuterSegmentToItsInnerOne)
{
	const auto cap = Build(k_Positions, k_Uvs, k_Normals, k_Indices, 0.25f, 0.35f);
	// Both triangles cross a quarter of the way up: two segments, two triangles each
	ASSERT_EQ(cap.size(), 12U);
	// The first triangle has only its third corner above: its segment runs from the edge off its first corner to the edge
	// off its second, as the game takes them
	EXPECT_FLOAT_EQ(cap[0].position.y, 0.25f);
	EXPECT_FLOAT_EQ(cap[0].position.z, 0.25f);
	EXPECT_FLOAT_EQ(cap[0].uv.y, 0.25f);
	EXPECT_FLOAT_EQ(cap[1].position.z, 1.0f);
	// The inner segment stands in by the inset across the ground, at the same height
	EXPECT_FLOAT_EQ(cap[2].position.x, 1.0f - 0.35f);
	EXPECT_FLOAT_EQ(cap[2].position.y, 0.25f);
	EXPECT_FLOAT_EQ(cap[2].position.z, cap[0].position.z);
	EXPECT_FLOAT_EQ(cap[3].position.x, 0.65f);
	EXPECT_FLOAT_EQ(cap[3].position.z, cap[1].position.z);
	// The quad's second triangle shares the outer segment's end and the inner one's start
	EXPECT_EQ(cap[4].position, cap[1].position);
	EXPECT_EQ(cap[5].position, cap[2].position);
}

TEST(PartialBuildCap, ACornerAtTheCutCountsAsAboveIt)
{
	// Cut at the top of the wall: its top corners are above, so both triangles still cross
	EXPECT_EQ(Build(k_Positions, k_Uvs, k_Normals, k_Indices, 1.0f, 0.2f).size(), 12U);
	// Cut at its foot, every corner is above and nothing is capped
	EXPECT_TRUE(Build(k_Positions, k_Uvs, k_Normals, k_Indices, 0.0f, 0.2f).empty());
	EXPECT_TRUE(Build(k_Positions, k_Uvs, k_Normals, k_Indices, 2.0f, 0.2f).empty());
}

TEST(PartialBuildCap, TheListHoldsAThousandSegmentsOfBothWalls)
{
	// The game's list stores while it holds at most 0x3E8, so 500 cut a wall is the most it caps
	static_assert(k_MostStoredSegments == 0x3E8);
	static_assert(k_MostSegments == 500);
	SegmentList<int> list;
	for (int i = 0; i <= static_cast<int>(k_MostStoredSegments); ++i)
	{
		list.Record(i, i);
	}
	EXPECT_EQ(list.Size(), k_MostStoredSegments + 1);
	EXPECT_FALSE(list.Overflowed());
	list.Record(0, 0);
	EXPECT_EQ(list.Size(), k_MostStoredSegments + 1);
	EXPECT_TRUE(list.Overflowed());
}

TEST(PartialBuildCap, CappedOnlyWhenBothWallsCutAsOftenAndNoOverflow)
{
	EXPECT_TRUE(Capped(3, 3, false));
	EXPECT_FALSE(Capped(0, 0, false));
	EXPECT_FALSE(Capped(3, 2, false));
	EXPECT_FALSE(Capped(3, 3, true));
}

TEST(PartialBuildCap, EachTriangleIsCutAlongTheGamesEdges)
{
	// By which corners (a, b, c) are above: the corner below and the corner above of each end, as the game takes them
	const auto edges = [](bool a, bool b, bool c) { return CutEdgesOf({a, b, c}); };
	const auto is = [](const std::optional<std::array<CutEdge, 2>>& got, std::array<int, 4> want) {
		return got.has_value() && (*got)[0].below == want[0] && (*got)[0].above == want[1] && (*got)[1].below == want[2] &&
		       (*got)[1].above == want[3];
	};
	EXPECT_FALSE(edges(false, false, false).has_value());
	EXPECT_FALSE(edges(true, true, true).has_value());
	EXPECT_TRUE(is(edges(true, true, false), {2, 0, 2, 1}));
	EXPECT_TRUE(is(edges(true, false, true), {1, 0, 1, 2}));
	EXPECT_TRUE(is(edges(true, false, false), {1, 0, 2, 0}));
	EXPECT_TRUE(is(edges(false, true, true), {0, 1, 0, 2}));
	EXPECT_TRUE(is(edges(false, true, false), {0, 1, 2, 1}));
	EXPECT_TRUE(is(edges(false, false, true), {0, 2, 1, 2}));
}

TEST(PartialBuildCap, TheCutShareIsTheGamesBitForBit)
{
	// The game's own cut point asked for the same edges (below height, above height, cut): the share from the corner
	// below, the bits it gave
	struct Case
	{
		float below;
		float above;
		float height;
		uint32_t share;
	};
	constexpr std::array k_Cases {
	    Case {0.0f, 1.0f, 0.25f, 0x3E800000},
	    Case {0.0f, 3.0f, 1.0f, 0x3EAAAAAB},
	    Case {0.1f, 0.7f, 0.3f, 0x3EAAAAAB},
	    Case {-0.2f, 2.9f, 1.7f, 0x3F1CE73A},
	    // A corner on the cut is the end of the edge
	    Case {0.0f, 1.0f, 1.0f, 0x3F800000},
	    // Both corners on the wrong side: each kept a hair on its own side, half way
	    Case {2.0f, 0.5f, 1.0f, 0x3F000000},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(std::bit_cast<uint32_t>(CutShare(c.below, c.above, c.height)), c.share)
		    << c.below << " " << c.above << " " << c.height;
	}
}

TEST(PartialBuildCap, TheCapIsThreeQuartersOfTheObjectColourAsTheGameShadesIt)
{
	// The game's own cap draw asked for these object colours: the colour it gave every corner of the cap
	struct Case
	{
		uint32_t object;
		uint32_t cap;
	};
	constexpr std::array k_Cases {
	    Case {0x00000000, 0x00000000}, Case {0xFFFFFFFF, 0xFFBFBFBF}, Case {0x00FFFFFF, 0x00BFBFBF},
	    Case {0x80FF8001, 0x80BF6000}, Case {0x00808080, 0x00606060}, Case {0xFF010203, 0xFF000102},
	    Case {0x007F7F7F, 0x005F5F5F}, Case {0x12345678, 0x1227405A}, Case {0x00AB55CD, 0x00803F99},
	    Case {0xFFFEFDFC, 0xFFBEBDBD},
	};
	for (const auto& c : k_Cases)
	{
		EXPECT_EQ(CapColour(c.object), c.cap) << std::hex << c.object;
	}
	// (c 3) >> 2 in every channel of every byte, the alpha untouched
	for (uint32_t byte = 0; byte < 256; ++byte)
	{
		const uint32_t grey = 0x5A000000u | (byte << 16) | (byte << 8) | byte;
		const uint32_t shade = (byte * 3) >> 2;
		EXPECT_EQ(CapColour(grey), 0x5A000000u | (shade << 16) | (shade << 8) | shade) << byte;
	}
}

TEST(PartialBuildCap, NoSnowLiesOnAPartlyBuiltModel)
{
	// The game draws snow only on whole objects: a partly built model's walls and cap never take it
	EXPECT_TRUE(TakesSnow(true, true, false));
	EXPECT_FALSE(TakesSnow(true, true, true));
	EXPECT_FALSE(TakesSnow(true, false, false));
	EXPECT_FALSE(TakesSnow(false, true, false));
	EXPECT_FALSE(TakesSnow(false, false, true));
}

TEST(PartialBuildCap, TooManySegmentsShowNoCap)
{
	std::vector<uint16_t> indices;
	for (size_t i = 0; i < k_MostSegments; ++i)
	{
		indices.insert(indices.end(), k_Indices.begin(), k_Indices.begin() + 3);
	}
	EXPECT_EQ(Build(k_Positions, k_Uvs, k_Normals, indices, 0.5f, 0.2f).size(), k_MostSegments * 6);
	indices.insert(indices.end(), k_Indices.begin(), k_Indices.begin() + 3);
	EXPECT_TRUE(Build(k_Positions, k_Uvs, k_Normals, indices, 0.5f, 0.2f).empty());
}

TEST(PartialBuildCap, APrimitiveWithAnyCornerBelowTheCutHasWallsWithin)
{
	// The wall's triangles all straddle a cut half way up: the parts below are drawn, so it has walls within
	EXPECT_TRUE(DrawsAnythingBelow(k_Positions, k_Indices, 0.5f));
	EXPECT_TRUE(DrawsAnythingBelow(k_Positions, k_Indices, 1.5f));
	// Cut at its foot, nothing of it is below: a corner on the cut is not below it
	EXPECT_FALSE(DrawsAnythingBelow(k_Positions, k_Indices, 0.0f));
}
