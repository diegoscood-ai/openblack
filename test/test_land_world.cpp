/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land as the game builds it: on synthetic data the testbed's flat land with cells set by hand, on the game's data
// the story lands' own files. The story and playground lands all open with at most sixteen countries; the land stands
// at each cell corner's altitude, between corners on the cell's two triangles, split one way or the other by the cell;
// and next to the sea land of altitude 3 or less is at sea level. Every expected value is the original's
// (docs/wiki/project/progress/terrain/land_height.md, land_loading.md and countries.md).

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>
#include <array>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/FlatLand.h"
#include "3D/Implementations/LandIsland.h"
#include "3D/LandData.h"
#include "support/WorldHarness.h"

using namespace openblack;

namespace
{
/// The cell the tests set by hand on synthetic data, inside its block
constexpr glm::u16vec2 k_Cell {101, 101};

class LandWorld: public test::world::WorldHarness
{
protected:
	/// The land: the game's Land 1, or the flat land with the cell's corners at these altitudes and split or not
	[[nodiscard]] std::unique_ptr<LandIsland> Land(std::array<uint8_t, 4> corners = {20, 30, 40, 50}, bool split = false) const
	{
		if (GameData())
		{
			return std::make_unique<LandIsland>(*test::world::GamePath() / "Data" / "Landscape" / "Land1.lnd");
		}
		auto data = flat_land::Build();
		const auto cell = [&data](uint16_t x, uint16_t z) -> lnd::LNDCell& {
			const auto block = data.blockIndexLookup.at(static_cast<size_t>((x >> 4) * LandData::k_BlocksPerSide + (z >> 4)));
			return data.blocks.at(block - 1u).cells.at(static_cast<size_t>((x & 0xF) * 17 + (z & 0xF)));
		};
		cell(k_Cell.x, k_Cell.y).altitude = corners[0];
		cell(k_Cell.x, k_Cell.y + 1).altitude = corners[1];
		cell(k_Cell.x + 1, k_Cell.y).altitude = corners[2];
		cell(k_Cell.x + 1, k_Cell.y + 1).altitude = corners[3];
		cell(k_Cell.x, k_Cell.y).properties.split = split ? 1 : 0;
		return std::make_unique<LandIsland>(data);
	}

	/// The cells to sample: on synthetic data the one set by hand; on the game's data every cell of Land 1 whose four
	/// corners stand above the sea's flattening
	[[nodiscard]] static std::vector<glm::u16vec2> HighCells(const LandIsland& land)
	{
		std::vector<glm::u16vec2> found;
		for (uint16_t x = 0; x + 1 < land.GetCellsPerSide(); x += 7)
		{
			for (uint16_t z = 0; z + 1 < land.GetCellsPerSide(); z += 7)
			{
				if (!land.HasBlockAt({x, z}) || !land.HasBlockAt({x + 1, z + 1}))
				{
					continue;
				}
				const auto low = std::min(
				    {Altitude(land, x, z), Altitude(land, x, z + 1), Altitude(land, x + 1, z), Altitude(land, x + 1, z + 1)});
				if (low > 4)
				{
					found.emplace_back(x, z);
				}
			}
		}
		return found;
	}

	[[nodiscard]] static uint16_t Altitude(const LandIsland& land, uint16_t x, uint16_t z)
	{
		return land.GetCellAltitude(land.GetCell({x, z}));
	}
	[[nodiscard]] static float Height(uint16_t altitude) { return static_cast<float>(altitude) * LandIsland::k_HeightUnit; }
	/// The point of the cell, `along` of the way in x and z
	[[nodiscard]] static glm::vec2 PointIn(glm::u16vec2 cell, float along)
	{
		return {(static_cast<float>(cell.x) + along) * 10.0f, (static_cast<float>(cell.y) + along) * 10.0f};
	}
};
} // namespace

// The five story lands and the tutorial land all open, each with at most sixteen countries
TEST_P(LandWorld, TheStoryLandsAllOpenWithAtMostSixteenCountries)
{
	if (!GameData())
	{
		GTEST_SKIP() << "the shipped landscape files: no synthetic data stands for them";
	}
	for (const auto* name : {"Land1.lnd", "Land2.lnd", "Land3.lnd", "Land4.lnd", "Land5.lnd", "LandT.lnd"})
	{
		std::unique_ptr<LandIsland> land;
		ASSERT_NO_THROW(land = std::make_unique<LandIsland>(*test::world::GamePath() / "Data" / "Landscape" / name)) << name;
		EXPECT_FALSE(land->GetCountries().empty()) << name;
		EXPECT_LE(land->GetCountries().size(), 16u) << name;
	}
}

// The land stands at each cell corner's altitude, and in the middle of a cell on the diagonal the cell splits it by:
// halfway between the corners at its ends
TEST_P(LandWorld, TheLandStandsAtItsCornersAndOnTheCellsSplitDiagonal)
{
	for (const bool split : {false, true})
	{
		const auto land = Land({20, 30, 40, 50}, split);
		const auto cells = GameData() ? HighCells(*land) : std::vector {k_Cell};
		ASSERT_FALSE(cells.empty());
		for (const auto cell : cells)
		{
			const auto a00 = Altitude(*land, cell.x, cell.y);
			const auto a01 = Altitude(*land, cell.x, cell.y + 1);
			const auto a10 = Altitude(*land, cell.x + 1, cell.y);
			const auto a11 = Altitude(*land, cell.x + 1, cell.y + 1);
			EXPECT_FLOAT_EQ(land->GetHeightAt(PointIn(cell, 0.0f)), Height(a00));
			const bool cellSplit = land->GetCell(cell).properties.split != 0;
			const float middle = cellSplit ? 0.5f * (Height(a01) + Height(a10)) : 0.5f * (Height(a00) + Height(a11));
			EXPECT_FLOAT_EQ(land->GetHeightAt(PointIn(cell, 0.5f)), middle) << cell.x << ", " << cell.y;
		}
		if (GameData())
		{
			break; // the game's land is the same either way
		}
	}
}

// Next to the sea land of altitude 3 or less stands at sea level; the land's own height keeps it
TEST_P(LandWorld, NextToTheSeaLowLandStandsAtSeaLevel)
{
	// a corner at the sea's edge (4), two of altitude 3 and one high, split across the two low ones
	const auto land = Land({4, 3, 3, 50}, true);
	const auto cell = k_Cell;
	if (GameData())
	{
		// every corner of the land at 3 or less, by the sea, is at sea level
		size_t low = 0;
		for (uint16_t x = 0; x < land->GetCellsPerSide(); x += 3)
		{
			for (uint16_t z = 0; z < land->GetCellsPerSide(); z += 3)
			{
				if (!land->HasBlockAt({x, z}))
				{
					continue;
				}
				const auto altitude = Altitude(*land, x, z);
				if (altitude > 0 && altitude <= 3)
				{
					++low;
					EXPECT_EQ(land->GetHeightAt(PointIn({x, z}, 0.0f)), 0.0f) << x << ", " << z;
					EXPECT_FLOAT_EQ(land->GetUnflattenedHeightAt(PointIn({x, z}, 0.0f)), Height(altitude));
				}
			}
		}
		EXPECT_GT(low, 0u);
		return;
	}
	EXPECT_EQ(land->GetHeightAt(PointIn(cell, 0.5f)), 0.0f);
	EXPECT_FLOAT_EQ(land->GetUnflattenedHeightAt(PointIn(cell, 0.5f)), Height(3));
}

INSTANTIATE_TEST_SUITE_P(Data, LandWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
