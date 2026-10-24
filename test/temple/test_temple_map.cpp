/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <map>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "3D/TempleMap.h"

using namespace openblack;

namespace
{
/// Land of a few blocks, each of its cells at an altitude and brightness, with the blocks from (2, 3) to (5, 7)
struct FakeLand
{
	uint8_t altitude {100};
	uint8_t luminosity {128};
	std::map<std::pair<int, int>, lnd::LNDCell> cells;

	[[nodiscard]] TempleMap::FindCell Finder()
	{
		return [this](glm::u16vec2 cell) -> const lnd::LNDCell* {
			const int blockX = cell.x / 16;
			const int blockZ = cell.y / 16;
			if (blockX < 2 || blockX > 5 || blockZ < 3 || blockZ > 7)
			{
				return nullptr;
			}
			auto& found = cells[{cell.x, cell.y}];
			found.altitude = altitude;
			found.luminosity = luminosity;
			return &found;
		};
	}
};

/// Our land as the map reads it through TempleMap::CellsOf: the same blocks as FakeLand, and an empty cell where no
/// block is, as our land gives one
class BlockIsland final: public LandIslandInterface
{
public:
	BlockIsland()
	{
		_cell.altitude = 100;
		_cell.luminosity = 128;
		_empty.altitude = 200;
		_empty.properties.unused6 = true;
	}

	[[nodiscard]] const lnd::LNDCell& Cell() const { return _cell; }

	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		return HasBlockAt(cell) ? _cell : _empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final
	{
		const int blockX = cell.x / 16;
		const int blockZ = cell.y / 16;
		return blockX >= 2 && blockX <= 5 && blockZ >= 3 && blockZ <= 7;
	}
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	lnd::LNDCell _cell {};
	lnd::LNDCell _empty {};
	std::vector<lnd::LNDCountry> _countries;
};
} // namespace

TEST(TempleMap, FramesTheIslandWithAnElevenUnitDiagonal)
{
	FakeLand land;
	TempleMap map;
	ASSERT_TRUE(map.Frame(land.Finder()));
	// 3 by 4 blocks between the first and last, so a scale of 11 / 5 a vertex
	EXPECT_FLOAT_EQ(map.GetWorldScale(), (11.0f / 5.0f) / 80.0f);
	// The middle of the blocks there are is at the room's centre
	const auto middle = map.ToMap(glm::vec2(4.0f, 5.5f) * 160.0f);
	EXPECT_NEAR(middle.x, 0.0f, 1e-4f);
	EXPECT_NEAR(middle.z, 0.0f, 1e-4f);
}

TEST(TempleMap, FramesAfreshForEachVisit)
{
	// Each visit frames the map on the blocks the land has then, not on those of an earlier visit
	FakeLand land;
	TempleMap map;
	ASSERT_TRUE(map.Frame(land.Finder()));
	lnd::LNDCell cell {};
	const auto twoByTwo = [&cell](glm::u16vec2 at) -> const lnd::LNDCell* {
		return at.x / 16 <= 1 && at.y / 16 <= 1 ? &cell : nullptr;
	};
	ASSERT_TRUE(map.Frame(twoByTwo));
	EXPECT_FLOAT_EQ(map.GetWorldScale(), (11.0f / std::sqrt(2.0f)) / 80.0f);
	const auto middle = map.ToMap(glm::vec2(160.0f, 160.0f));
	EXPECT_NEAR(middle.x, 0.0f, 1e-4f);
	EXPECT_NEAR(middle.z, 0.0f, 1e-4f);
}

TEST(TempleMap, HasNoFrameWithoutLand)
{
	TempleMap map;
	EXPECT_FALSE(map.Frame([](glm::u16vec2) -> const lnd::LNDCell* { return nullptr; }));
	std::vector<OrientedTextVertex> triangles;
	map.Build([](glm::u16vec2) -> const lnd::LNDCell* { return nullptr; }, triangles);
	EXPECT_TRUE(triangles.empty());
}

TEST(TempleMap, DrawsTwoTrianglesAQuadOfEachBlock)
{
	FakeLand land;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	// 4 by 5 blocks of 2 by 2 quads
	EXPECT_EQ(triangles.size(), 4 * 5 * 4 * 2 * 3);

	// Land is grey by its brightness, opaque above the coast, and as high as the world scaled
	const auto& vertex = triangles[1];
	EXPECT_EQ(vertex.colour, 0xFF7F7F7Fu);
	EXPECT_NEAR(vertex.position.y, 100.0f * 0.67f * map.GetWorldScale(), 1e-5f);
	// Its texture spans the 32 blocks there can be
	EXPECT_EQ(vertex.uv * 64.0f, glm::round(vertex.uv * 64.0f));
}

TEST(TempleMap, DrawsEachQuadsTrianglesInTheGamesOrder)
{
	FakeLand land;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	ASSERT_GE(triangles.size(), 6u);
	// The first quad's, at the land's first block (2, 3), two vertices a block: (x, z) (x + 1, z + 1) (x + 1, z), then
	// (x + 1, z + 1) (x, z) (x, z + 1), each corner told by its texture point, a 64th of the map a vertex
	const glm::vec2 first(4.0f, 6.0f);
	const std::array<glm::vec2, 6> corners {glm::vec2(0, 0), glm::vec2(1, 1), glm::vec2(1, 0),
	                                        glm::vec2(1, 1), glm::vec2(0, 0), glm::vec2(0, 1)};
	for (size_t i = 0; i < corners.size(); ++i)
	{
		EXPECT_EQ(triangles[i].uv * 64.0f, first + corners.at(i)) << i;
	}
}

TEST(TempleMap, FadesAtTheCoast)
{
	FakeLand land;
	land.altitude = 2;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	EXPECT_EQ(triangles[1].colour >> 24, 0xAAu);
}

TEST(TempleMap, MapsTheWorldBackAndForth)
{
	FakeLand land;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	const glm::vec2 world(700.0f, 900.0f);
	const auto onMap = map.ToMap(world);
	EXPECT_NEAR(onMap.y, 100.0f * 0.67f * map.GetWorldScale(), 1e-4f);
	const auto back = map.ToWorld(onMap);
	EXPECT_NEAR(back.x, world.x, 1e-2f);
	EXPECT_NEAR(back.y, world.y, 1e-2f);
}

TEST(TempleMap, ColoursMarkersByTheirPlayersLightened)
{
	// Player one's red, a quarter of the way to white
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_ONE, 0), glm::u8vec3(0xFF, 0x46 + 0x2E, 0x46 + 0x2E));
	// The neutral player's black is white, as are the things of no player
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::NEUTRAL, 0), glm::u8vec3(0xFF));
	EXPECT_EQ(TempleMap::MarkerColour(std::nullopt, 0), glm::u8vec3(0xFF));
}

TEST(TempleMap, GlowsUnderTheMarkersInAWarmGrey)
{
	// The game's colour bytes run blue, green, red: 0x61, 0x6E, 0x7C is red 0x7C, green 0x6E and blue 0x61
	EXPECT_EQ(TempleMap::k_MarkerGlow, glm::u8vec3(0x7C, 0x6E, 0x61));
}

TEST(TempleMap, ColoursMarkersAsTheLandGivesItsPlayers)
{
	// On the first land the second player is in the seventh player's orange
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_TWO, 1), glm::u8vec3(0xFF, 0xA2 + 0x17, 0x47 + 0x2E));
	// On the second land the second and third are yellow and blue; elsewhere they keep their own
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_TWO, 2), glm::u8vec3(0xFF, 0xFD, 0x47 + 0x2E));
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_THREE, 2), glm::u8vec3(0x47 + 0x2E, 0x77 + 0x22, 0xFF));
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_TWO, 3), glm::u8vec3(0x47 + 0x2E, 0x77 + 0x22, 0xFF));
	EXPECT_EQ(TempleMap::MarkerColour(PlayerNames::PLAYER_TWO, 0), glm::u8vec3(0x47 + 0x2E, 0xFF, 0x54 + 0x2A));
}

TEST(TempleMap, PutsTheInfluenceOnTheMapAtItsScale)
{
	FakeLand land;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	const auto placement = map.GetInfluencePlacement();
	EXPECT_EQ(placement.offset, map.ToMap(glm::vec2(0.0f)));
	EXPECT_FLOAT_EQ(placement.scale, map.GetWorldScale());
	// A point of the world stands over its place on the map, as high above the world's origin there as its height
	// scaled
	const glm::vec3 world(700.0f, 40.0f, 900.0f);
	const auto room = placement.ToRoom(world);
	const auto onMap = map.ToMap(glm::vec2(world.x, world.z));
	EXPECT_NEAR(room.x, onMap.x, 1e-4f);
	EXPECT_NEAR(room.z, onMap.z, 1e-4f);
	EXPECT_NEAR(room.y, placement.offset.y + (40.0f * placement.scale), 1e-5f);
}

TEST(TempleMap, StandsMarkersOnTheCellOfTheirThings)
{
	FakeLand land;
	TempleMap map;
	map.Frame(land.Finder());
	std::vector<OrientedTextVertex> triangles;
	map.Build(land.Finder(), triangles);
	EXPECT_EQ(map.MarkerPosition(glm::vec2(705.0f, 909.9f)), map.ToMap(glm::vec2(700.0f, 900.0f)));
}

TEST(TempleMap, ReadsOurLandOnlyWhereItHasBlocks)
{
	const BlockIsland island;
	const auto findCell = TempleMap::CellsOf(island);
	EXPECT_EQ(findCell(glm::u16vec2(2 * 16, 3 * 16)), &island.Cell());
	EXPECT_EQ(findCell(glm::u16vec2(5 * 16 + 15, 7 * 16 + 15)), &island.Cell());
	// The empty cell our land gives where there is no block is not land on the map
	EXPECT_EQ(findCell(glm::u16vec2(0, 0)), nullptr);
	EXPECT_EQ(findCell(glm::u16vec2(6 * 16, 3 * 16)), nullptr);

	// So the map is framed and built as on the same blocks given cell by cell
	FakeLand land;
	TempleMap fromCells;
	TempleMap fromLand;
	ASSERT_TRUE(fromCells.Frame(land.Finder()));
	ASSERT_TRUE(fromLand.Frame(findCell));
	EXPECT_FLOAT_EQ(fromLand.GetWorldScale(), fromCells.GetWorldScale());
	std::vector<OrientedTextVertex> cellTriangles;
	std::vector<OrientedTextVertex> landTriangles;
	fromCells.Build(land.Finder(), cellTriangles);
	fromLand.Build(findCell, landTriangles);
	ASSERT_EQ(landTriangles.size(), cellTriangles.size());
	for (size_t i = 0; i < landTriangles.size(); ++i)
	{
		EXPECT_EQ(landTriangles[i].position, cellTriangles[i].position) << i;
		EXPECT_EQ(landTriangles[i].colour, cellTriangles[i].colour) << i;
	}
}

// A double click on the map near its middle leaves for the place whatever the land there, even the sea, the click's
// height in the room not counted; the camera goes 50 above the place and 70 along z from it, looking at it
TEST(TempleMap, LeavesForAPlaceNearTheMiddleWhateverTheLand)
{
	const glm::vec2 world {1250.5f, 2730.25f};
	const auto leave = TempleMap::LeaveFor(glm::vec3(3.0f, 40.0f, 4.0f), world, -2.5f);
	ASSERT_TRUE(leave.has_value());
	EXPECT_EQ(leave->origin, glm::vec3(1250.5f, -2.5f + 50.0f, 2730.25f + 70.0f));
	EXPECT_EQ(leave->focus, glm::vec3(1250.5f, -2.5f, 2730.25f));
}

// From 10 of the middle on, only land higher than 1 is taken
TEST(TempleMap, FartherOutLeavesOnlyForLandHigherThanOne)
{
	const glm::vec2 world {800.0f, 600.0f};
	// 10 from the middle, exactly
	const glm::vec3 atTen {6.0f, 0.0f, 8.0f};
	EXPECT_FALSE(TempleMap::LeaveFor(atTen, world, 0.0f).has_value());
	EXPECT_FALSE(TempleMap::LeaveFor(atTen, world, 1.0f).has_value());
	const float aboveOne = std::nextafter(1.0f, 2.0f);
	const auto leave = TempleMap::LeaveFor(atTen, world, aboveOne);
	ASSERT_TRUE(leave.has_value());
	EXPECT_EQ(leave->origin, glm::vec3(800.0f, aboveOne + 50.0f, 670.0f));
	EXPECT_EQ(leave->focus, glm::vec3(800.0f, aboveOne, 600.0f));
	// Inside 10 the sea is taken too
	EXPECT_TRUE(TempleMap::LeaveFor(glm::vec3(6.0f, 0.0f, 7.99f), world, 0.0f).has_value());
}
