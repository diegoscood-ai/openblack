/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The water test under a body (Body::TouchGround, through the island's PhysicsGround): the cell rounded to the
// nearest, and no cell is water

#include <cstdint>

#include <array>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/PhysicsGround.h"
#include "Physics/Body.h"
#include "Physics/Ground.h"

using namespace openblack;
using openblack::ecs::PhysicsGround;

namespace
{
/// Just the cells: blocks of 16 x 16 cells on a grid of `blocksPerSide`, missing blocks allowed
class CellIsland final: public LandIslandInterface
{
public:
	explicit CellIsland(uint16_t blocksPerSide)
	    : _blocksPerSide(blocksPerSide)
	    , _cells(static_cast<size_t>(blocksPerSide) * 16 * blocksPerSide * 16)
	    , _hasBlock(static_cast<size_t>(blocksPerSide) * blocksPerSide, false)
	{
	}

	void SetBlock(glm::u16vec2 block, bool present)
	{
		_hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y] = present;
	}
	lnd::LNDCell& Cell(glm::u16vec2 cell) { return _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y]; }

	/// the interpolated height is not modelled: a marker value so that the tests see when it is asked for
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return k_HeightMarker; }
	static constexpr float k_HeightMarker = 123.0f;
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& cell) const final
	{
		static const lnd::LNDCell k_Empty {};
		return HasBlockAt(cell) ? _cells[static_cast<size_t>(cell.x) * _blocksPerSide * 16 + cell.y] : k_Empty;
	}
	[[nodiscard]] bool HasBlockAt(const glm::u16vec2& cell) const final
	{
		const auto block = cell >> static_cast<uint16_t>(4);
		return block.x < _blocksPerSide && block.y < _blocksPerSide &&
		       _hasBlock[static_cast<size_t>(block.x) * _blocksPerSide + block.y];
	}
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return static_cast<uint16_t>(_blocksPerSide * 16); }
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
	uint16_t _blocksPerSide;
	std::vector<lnd::LNDCell> _cells;
	std::vector<bool> _hasBlock;
	std::vector<lnd::LNDCountry> _countries;
};

class PhysicsBodyWaterTest: public ::testing::Test
{
protected:
	void SetUp() override { _island.SetBlock({0, 0}, true); }

	/// Land under a point: the nearest cell is not open sea
	[[nodiscard]] bool IsLandUnder(glm::vec3 point) const
	{
		return !PhysicsGround(&_island).IsSeaCell(glm::vec2(point.x, point.z));
	}

	CellIsland _island {static_cast<uint16_t>(2)};
};
} // namespace

TEST_F(PhysicsBodyWaterTest, NearestCellDecides)
{
	// cell (3, 4) is land, its neighbours are sea (altitude 0)
	_island.Cell({3, 4}).altitude = 5;
	// x 0.1 = 3.4 / z 0.1 = 4.4: rounds to (3, 4)
	EXPECT_TRUE(IsLandUnder({34.0f, 0.0f, 44.0f}));
	// 2.6 / 3.6 round to (3, 4) too, although truncating would give (2, 3)
	EXPECT_TRUE(IsLandUnder({26.0f, 0.0f, 36.0f}));
	// 3.6 rounds to 4: the sea cell (4, 4)
	EXPECT_FALSE(IsLandUnder({36.0f, 0.0f, 44.0f}));
}

TEST_F(PhysicsBodyWaterTest, NoCellIsWater)
{
	_island.Cell({0, 0}).altitude = 5;
	// off the map is water, not the edge cell
	EXPECT_FALSE(IsLandUnder({-20.0f, 0.0f, 0.0f}));
	// a cell of a missing block (block (1, 0)) is water
	EXPECT_FALSE(IsLandUnder({200.0f, 0.0f, 0.0f}));
	EXPECT_TRUE(IsLandUnder({1.0f, 0.0f, 1.0f}));
}

namespace
{
/// The open sea with its floor at the sea's level: every cell is sea
class OpenSea final: public physics::Ground
{
public:
	[[nodiscard]] float HeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 NormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] bool IsSeaCell(glm::vec2) const final { return true; }
};

/// A still body of three points and one face, radius 1, its centre at that height in the sea
physics::Body StillBodyAt(float height)
{
	constexpr std::array<glm::vec3, 3> k_Local = {glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.5f),
	                                              glm::vec3(0.0f, 0.0f, -1.0f)};
	constexpr std::array<std::array<uint32_t, 3>, 1> k_Triangles = {{{0, 1, 2}}};
	constexpr physics::PhysicsData k_Data = {0.8f, 300.0f, 30.0f, 0.5f, 0.3f, 0.1f};
	physics::Body body;
	body.Initialise(1.0f, 1.0f);
	body.SetUpConstants(5.0f, k_Data, true);
	body.BuildShape(k_Local, k_Triangles, glm::vec3(0.0f), 1.0f, 1.0f, glm::mat3(1.0f), glm::vec3(100.0f, height, 100.0f));
	return body;
}
} // namespace

TEST(PhysicsBodyInTheSea, ItIsLiftedByItsSunkShareOfItsWeightOverItsDensityAndSoaksUpWater)
{
	// centre 0.2 under the sea, radius 1: (1 + 0.2) / 2 of it is under; it grows denser by a fifteen-thousandth first
	auto body = StillBodyAt(-0.2f);
	const float density = body.density;
	body.ClearForces();
	body.TouchGround(OpenSea());
	EXPECT_TRUE(body.inWater);
	EXPECT_EQ(body.density, density + physics::k_SoakPerStep);
	const float fraction = (1.0f - -0.2f) / (2.0f * 1.0f);
	const float lift = fraction * 5.0f * physics::k_Gravity / body.density;
	EXPECT_EQ(body.Force().y, (0.0f - 5.0f * physics::k_Gravity) + lift);
	// with every point under the sea it touches no floor
	EXPECT_EQ(body.Contacts(), 0);
}

TEST(PhysicsBodyInTheSea, DeepDownAtMostItsWholeWeightOverItsDensityLiftsIt)
{
	auto body = StillBodyAt(-5.0f);
	body.ClearForces();
	body.TouchGround(OpenSea());
	EXPECT_EQ(body.Force().y, (0.0f - 5.0f * physics::k_Gravity) + 1.0f * 5.0f * physics::k_Gravity / body.density);
	EXPECT_EQ(body.Contacts(), 0);
}
