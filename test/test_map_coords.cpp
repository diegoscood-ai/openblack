/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The map position API (ECS/MapCoords: MapCoords::Set 0x603340, GetLHPoint 0x605C40, ToMap 0x603430, InBounds 0x6042C0,
// operator+=(JustMapXZ) 0x605470, GUtils::Spiral 0x74D7E0, SpiralIncrement 0x74D810 and the spiral sizes 0x74F520 /
// 0x74F540) against values worked out with 24-bit float products.

#include <algorithm>
#include <bit>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/MapCoords.h"

namespace mc = openblack::ecs::map_coords;

TEST(MapCoords, Constants)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::k_FixedPerMetre), 0x45CCCCCDu);  // [0x8AC400]
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::k_MetresPerFixed), 0x39200000u); // [0x8AA3A4]
	EXPECT_EQ(mc::k_MapCells, 512u);
}

TEST(MapCoords, ToFixedTruncatesTheFloatProduct)
{
	// 1464 * 6553.60009765625 = 9594470.54..., rounded to float 9594471, truncated
	EXPECT_EQ(mc::ToFixed(1464.0f), 9594471);
	EXPECT_EQ(mc::ToFixed(1469.9f), 9633137);
	EXPECT_EQ(mc::ToFixed(1234.567f), 8090858);
	EXPECT_EQ(mc::ToFixed(0.3f), 1966);
	EXPECT_EQ(mc::ToFixed(-0.3f), -1966); // towards 0
	EXPECT_EQ(mc::ToFixed(-1e-4f), 0);    // (-1.5e-4, 0) truncates to 0: inside the map
	EXPECT_EQ(mc::ToFixed(10.0f), 0x10000);
	EXPECT_EQ(mc::ToFixed(5119.99f), 33554368);
}

TEST(MapCoords, ToFixedGUtilsIsAnotherRounding)
{
	// 1464 * 65536 / 10 = 9594470.4 -> 9594470, one unit below ToFixed
	EXPECT_EQ(mc::ToFixedGUtils(1464.0f), 9594470);
	EXPECT_EQ(mc::ToFixedGUtils(5000.0f), 32768000);
	EXPECT_EQ(mc::ToFixedGUtils(2.5f), 16384);
}

TEST(MapCoords, ToMetresRoundsOnce)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::ToMetres(0x10000)), 0x41200000u);    // 10
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::ToMetres(9594634)), 0x44B700CCu);    // 1464.0249
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::ToMetres(-1966)), 0xBE999800u);      // -0.29998779
	// above 2^24 a float of the integer would round first: (float)16777217 * k gives 0x45200000
	EXPECT_EQ(std::bit_cast<uint32_t>(mc::ToMetres(16777217)), 0x45200001u);
	EXPECT_EQ(mc::Quantise(1464.0f), mc::ToMetres(9594471));
	// a round trip is not idempotent: GetLHPoint then MapCoords(LHPoint) can lose one unit, as in the original
	EXPECT_EQ(mc::ToFixed(mc::ToMetres(8090858)), 8090857);
}

TEST(MapCoords, CellsAndInBounds)
{
	EXPECT_EQ(mc::CellOf(glm::vec3(1464.0f, 0.0f, 2016.0f)), glm::ivec2(146, 201));
	EXPECT_EQ(mc::CellOf(glm::vec3(1469.9f, 0.0f, 2019.9f)), glm::ivec2(146, 201));
	EXPECT_EQ(mc::CellOf(mc::ToFixed(-0.3f)), 0xFFFF);
	EXPECT_EQ(mc::SignedCellOf(mc::ToFixed(-0.3f)), -1);
	EXPECT_EQ(mc::CellOf(mc::ToFixed(-1e-4f)), 0);

	EXPECT_TRUE(mc::InBounds(glm::vec3(0.0f, 0.0f, 0.0f)));
	EXPECT_TRUE(mc::InBounds(glm::vec3(-1e-4f, 0.0f, 5119.99f)));
	EXPECT_FALSE(mc::InBounds(glm::vec3(-0.3f, 0.0f, 100.0f)));
	EXPECT_FALSE(mc::InBounds(glm::vec3(100.0f, 0.0f, 5120.0f)));
	EXPECT_TRUE(mc::InBounds(glm::ivec2(511, 0)));
	EXPECT_FALSE(mc::InBounds(glm::ivec2(-1, 0)));
	EXPECT_FALSE(mc::InBounds(glm::ivec2(0, 512)));
	EXPECT_FALSE(mc::InBounds(glm::ivec2(100, 100), 64));

	const mc::MapCoords coords {mc::ToFixed(1464.0f), mc::ToFixed(2016.0f), 0.0f};
	EXPECT_EQ(mc::CellX(coords), 146);
	EXPECT_EQ(mc::CellZ(coords), 201);
	EXPECT_EQ(mc::CellIndex(coords), 146 * 512 + 201);
	EXPECT_EQ(mc::CellIndex(mc::MapCoords {-1, 0, 0.0f}), -1);
}

TEST(MapCoords, AddCellsKeepsTheFraction)
{
	mc::MapCoords coords {0x00921234, 0x00C95678, 3.0f};
	mc::AddCells(coords, {-1, 2});
	EXPECT_EQ(coords.x, 0x00911234);
	EXPECT_EQ(coords.z, 0x00CB5678);
	EXPECT_EQ(coords.altitude, 3.0f);
	mc::MapCoords edge {0x00001234, 0, 0.0f};
	mc::AddCells(edge, {-1, 0}); // the 16-bit add wraps to 0xFFFF: off the map
	EXPECT_EQ(static_cast<uint32_t>(edge.x), 0xFFFF1234u);
	EXPECT_FALSE(mc::InBounds(edge));
}

TEST(MapCoords, Neighbours)
{
	const std::array<mc::JustMapXZ, 4> n4 {{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}}; // 0x74CA10
	EXPECT_EQ(mc::k_Neighbours4, n4);
	const std::array<mc::JustMapXZ, 9> n8 {{{1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}, {0, -1}, {1, -1}, {0, 0}}};
	EXPECT_EQ(mc::k_Neighbours8, n8); // 0x74CA60
}

TEST(MapCoords, SpiralSequence)
{
	// from dir = count = 1: (-1,0), (0,-1), (1,0) x2, (0,1) x2, (-1,0) x3, (0,-1) x3, (1,0) x4
	const std::vector<mc::JustMapXZ> expected {{-1, 0}, {0, -1}, {1, 0},  {1, 0},  {0, 1},  {0, 1},  {-1, 0}, {-1, 0},
	                                           {-1, 0}, {0, -1}, {0, -1}, {0, -1}, {1, 0},  {1, 0},  {1, 0},  {1, 0}};
	mc::Spiral spiral;
	for (size_t i = 0; i < expected.size(); ++i)
	{
		EXPECT_EQ(spiral.Next(), expected[i]) << "step " << i;
	}

	// the centre plus 15 steps covers the 4 x 4 square from -2 to +1 around the centre
	mc::Spiral square;
	mc::MapCoords at {0x00640000, 0x00640000, 0.0f};
	std::vector<glm::ivec2> cells {mc::Cell(at)};
	for (int i = 1; i < 16; ++i)
	{
		mc::AddCells(at, square.Next());
		cells.push_back(mc::Cell(at));
	}
	for (int x = 98; x <= 101; ++x)
	{
		for (int z = 98; z <= 101; ++z)
		{
			EXPECT_NE(std::find(cells.begin(), cells.end(), glm::ivec2(x, z)), cells.end()) << x << "," << z;
		}
	}
}

TEST(MapCoords, SpiralIncrementAndSizes)
{
	mc::MapCoords coords {mc::ToFixed(1464.0f), mc::ToFixed(1464.0f), 0.0f};
	mc::Spiral spiral;
	mc::SpiralIncrement(coords, spiral, 2.5f); // first step (-1, 0): x - 2.5 m through GUtils' formula
	EXPECT_EQ(coords.x, 9578087);
	EXPECT_EQ(coords.z, mc::ToFixedGUtils(mc::ToMetres(9594471)));

	EXPECT_EQ(mc::CellSpiralSize(7.0f), 1);  // ftol(1.4) = 1
	EXPECT_EQ(mc::CellSpiralSize(2.0f), 1);  // 0 -> 1
	EXPECT_EQ(mc::CellSpiralSize(25.0f), 25); // 5^2
	EXPECT_EQ(mc::CellSpiralSize(-10.0f), 4); // unsigned jae: -2 stays, (-2)^2
	EXPECT_EQ(mc::IncrementSpiralSize(12.0f, 5.0f), 25); // n = ftol(-4.8) = -4: (1 + 4)^2
	EXPECT_EQ(mc::IncrementSpiralSize(1.0f, 5.0f), 1);   // n = 0
}

TEST(MapCoords, WorldWithoutIsland)
{
	const auto coords = mc::FromWorld(nullptr, glm::vec3(1464.0f, 7.0f, 0.3f));
	EXPECT_EQ(coords.x, 9594471);
	EXPECT_EQ(coords.z, 1966);
	EXPECT_EQ(coords.altitude, 7.0f);
	const auto back = mc::ToWorld(nullptr, coords);
	EXPECT_EQ(back.x, mc::ToMetres(9594471));
	EXPECT_EQ(back.y, 7.0f);
	EXPECT_EQ(back.z, mc::ToMetres(1966));
}
