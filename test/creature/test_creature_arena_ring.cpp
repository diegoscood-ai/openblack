/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The ring of light round a fight's arena (Creature/CreatureArenaRing.h): its points against the original's own (the
// oracle, 2026-10-29: four arenas, 32 points each, the first and last the same), and its height

#include <cstdint>

#include <array>
#include <bit>

#include <gtest/gtest.h>

#include "Creature/CreatureArenaRing.h"

using namespace openblack;
namespace ring = openblack::creature_arena_ring;

TEST(CreatureArenaRing, ThePointsAreTheOriginals)
{
	struct Case
	{
		uint32_t centreX, centreZ, radius, index, x, z;
	};
	constexpr std::array k_Cases {
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 0, 0x447f0000, 0x44fa0000},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 1, 0x447ee5cc, 0x44fa80d5},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 7, 0x447ac1d4, 0x44fc789f},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 8, 0x4479bf2b, 0x44fc7f2e},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 15, 0x44750691, 0x44fa40bf},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 16, 0x44750691, 0x44f9bf41},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 23, 0x4479bf2b, 0x44f780d2},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 30, 0x447ee5cc, 0x44f97f2b},
	    Case {0x447a0000, 0x44fa0000, 0x41a00000, 31, 0x447f0000, 0x44fa0000},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 0, 0x451d9a86, 0x452e0ca0},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 1, 0x451d8ebc, 0x452e8093},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 7, 0x451bb1bf, 0x453045fc},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 8, 0x451b3d59, 0x45304be3},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 15, 0x45191d7b, 0x452e46e6},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 16, 0x45191d7b, 0x452dd25a},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 23, 0x451b3d59, 0x452bcd5d},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 30, 0x451d8ebc, 0x452d98ad},
	    Case {0x451b5a86, 0x452e0ca0, 0x42100000, 31, 0x451d9a86, 0x452e0ca0},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 0, 0x4302f4bc, 0x44455000},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 1, 0x4302cd6f, 0x4445b0a0},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 7, 0x42f92ef4, 0x44472a77},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 8, 0x42f626fb, 0x44472f62},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 15, 0x42e7fd2d, 0x44458090},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 16, 0x42e7fd2d, 0x44451f70},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 23, 0x42f626fb, 0x4443709e},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 30, 0x4302cd6f, 0x4444ef60},
	    Case {0x42f6e979, 0x44455000, 0x40f00000, 31, 0x4302f4bc, 0x44455000},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 0, 0x451e459a, 0x452e4b50},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 1, 0x451e31f3, 0x452f0c8f},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 7, 0x451b16f9, 0x4532003e},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 8, 0x451a54fa, 0x45320a15},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 15, 0x4516ca87, 0x452eac6f},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 16, 0x4516ca87, 0x452dea31},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 23, 0x451a54fa, 0x452a8c8b},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 30, 0x451e31f3, 0x452d8a11},
	    Case {0x451a859a, 0x452e4b50, 0x42700000, 31, 0x451e459a, 0x452e4b50},
	};
	for (const auto& c : k_Cases)
	{
		const auto points =
		    ring::Points({std::bit_cast<float>(c.centreX), std::bit_cast<float>(c.centreZ)}, std::bit_cast<float>(c.radius));
		ASSERT_EQ(points.size(), ring::k_Points);
		EXPECT_EQ(std::bit_cast<uint32_t>(points.at(c.index).x), c.x) << c.radius << " " << c.index;
		EXPECT_EQ(std::bit_cast<uint32_t>(points.at(c.index).y), c.z) << c.radius << " " << c.index;
	}
}

TEST(CreatureArenaRing, ItStandsASixthOfTheRadiusHighInWhiteLight)
{
	EXPECT_EQ(ring::Height(36.0f), 36.0f * 0.16666667f);
	EXPECT_EQ(ring::Height(0.0f), 0.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring::k_Spread), 0x3F8147AEu);
	EXPECT_EQ(std::bit_cast<uint32_t>(ring::k_ShiftSeconds), 0x3CF5C28Fu);
	EXPECT_EQ(ring::k_Rgb, 0xFFFFFFu);
	EXPECT_EQ(ring::k_Points, 32u);
}
