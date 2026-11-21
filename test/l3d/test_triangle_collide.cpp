/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/TriangleCollide.h"

using namespace openblack;
using triangle_collide::First;
using triangle_collide::Range;

namespace
{
/// Two triangles across the z axis, one at z 10 and one at z 5. Wound so that (V1 - V0) x (V2 - V0) points to -z,
/// towards an eye at the origin looking along +z
constexpr std::array<glm::vec3, 6> k_Positions {{
    {-1.0f, -1.0f, 10.0f},
    {-1.0f, 1.0f, 10.0f},
    {1.0f, -1.0f, 10.0f},
    {-1.0f, -1.0f, 5.0f},
    {-1.0f, 1.0f, 5.0f},
    {1.0f, -1.0f, 5.0f},
}};
/// The far triangle first, then the near one
constexpr std::array<uint16_t, 6> k_FarThenNear {0, 1, 2, 3, 4, 5};
/// The same triangles wound the other way: their faces point away from the eye
constexpr std::array<uint16_t, 6> k_FarThenNearBack {0, 2, 1, 3, 5, 4};
constexpr glm::vec3 k_Eye {-0.5f, -0.5f, 0.0f};
constexpr glm::vec3 k_Ahead {0.0f, 0.0f, 1.0f};
} // namespace

TEST(TriangleCollide, TakesTheFirstTriangleItMeetsNotTheNearest)
{
	// one primitive holding both: the far one is listed first and taken
	const std::array ranges {Range {.first = 0, .count = 6, .twoSided = false}};
	const auto hit = First(k_Positions, k_FarThenNear, ranges, k_Eye, k_Ahead);
	ASSERT_TRUE(hit.has_value());
	EXPECT_FLOAT_EQ(hit->t, 10.0f);
	EXPECT_FLOAT_EQ(hit->normal.z, -1.0f);
	// the primitives in their order too: the near one's primitive first
	const std::array nearFirst {Range {.first = 3, .count = 3, .twoSided = false},
	                            Range {.first = 0, .count = 3, .twoSided = false}};
	EXPECT_FLOAT_EQ(First(k_Positions, k_FarThenNear, nearFirst, k_Eye, k_Ahead)->t, 5.0f);
}

TEST(TriangleCollide, LeavesOutTheBackFacesOfAOneSidedPrimitive)
{
	const std::array oneSided {Range {.first = 0, .count = 6, .twoSided = false}};
	EXPECT_FALSE(First(k_Positions, k_FarThenNearBack, oneSided, k_Eye, k_Ahead).has_value());
	// a two-sided primitive's count by either face, the first one listed again
	const std::array twoSided {Range {.first = 0, .count = 6, .twoSided = true}};
	EXPECT_FLOAT_EQ(First(k_Positions, k_FarThenNearBack, twoSided, k_Eye, k_Ahead)->t, 10.0f);
	// a back face left out lets the next primitive's front face be taken
	const std::array mixed {Range {.first = 0, .count = 3, .twoSided = false},
	                        Range {.first = 3, .count = 3, .twoSided = false}};
	const std::array<uint16_t, 6> farBackNearFront {0, 2, 1, 3, 4, 5};
	EXPECT_FLOAT_EQ(First(k_Positions, farBackNearFront, mixed, k_Eye, k_Ahead)->t, 5.0f);
}

TEST(TriangleCollide, MissesBesideAndBehind)
{
	const std::array ranges {Range {.first = 0, .count = 6, .twoSided = true}};
	EXPECT_FALSE(First(k_Positions, k_FarThenNear, ranges, {2.0f, 2.0f, 0.0f}, k_Ahead).has_value());
	EXPECT_FALSE(First(k_Positions, k_FarThenNear, ranges, {-0.5f, -0.5f, 20.0f}, k_Ahead).has_value());
}

TEST(TriangleCollide, OnlyTheDrawnLevelAndTheStatusPartsTheObjectShowsAreTested)
{
	using triangle_collide::Tested;
	// the first level of detail, alone or with others (a sub-mesh loaded as of every level has all three bits)
	EXPECT_TRUE(Tested(false, 0b001, 0, 0));
	EXPECT_TRUE(Tested(false, 0b111, 0, 0));
	// another level only, or none: never
	EXPECT_FALSE(Tested(false, 0b010, 0, 0));
	EXPECT_FALSE(Tested(false, 0b110, 0, 0));
	EXPECT_FALSE(Tested(false, 0b000, 0, 0));
	// a status part: only once the object's status shows it (a temple heart's shell at status 0 never; a graveyard's
	// graves up to its stage)
	EXPECT_FALSE(Tested(false, 0b001, 1, 0));
	EXPECT_TRUE(Tested(false, 0b001, 2, 2));
	EXPECT_FALSE(Tested(false, 0b001, 3, 2));
	// a physics sub-mesh: never
	EXPECT_FALSE(Tested(true, 0b111, 0, 0));
}
