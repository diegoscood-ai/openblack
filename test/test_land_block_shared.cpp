/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land blocks built from the same inputs share their vertex list (LandBlockMeshes). Each block's vertices must be
// byte for byte the ones the previous build made on its own, which this test keeps below as the reference: on a flat
// land, on a land of repeated blocks with a few varied corners (on block borders, in the ring only the normals read,
// in the noise only, open sea), at the map edge, with 10 altitude bits, and on a land where no two blocks are alike.

#include <cstdint>
#include <cstring>

#include <array>
#include <functional>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Graphics/Mesh.h"

using namespace openblack;

namespace
{
constexpr int k_Side = 17;
constexpr int k_CellsPerBlock = 16;

static_assert(sizeof(LandVertex) == 56, "LandVertex has no padding, so equal bytes are equal vertices");

using CellAt = std::function<lnd::LNDCell(int x, int z)>;
using NoiseAt = std::function<uint8_t(int x, int z)>;

/// Blocks on a grid of blocks.x x blocks.y from block (0, 0), on a map of cellsPerSide cells; every corner is kept in
/// each block that stores it, as in a land file
class SharedMeshIsland final: public LandIslandInterface
{
public:
	SharedMeshIsland(glm::ivec2 blocks, uint16_t cellsPerSide, uint8_t altitudeBits, const CellAt& cellAt,
	                 const NoiseAt& noiseAt, std::mt19937& random)
	    : _blocks(blocks)
	    , _cellsPerSide(cellsPerSide)
	    , _altitudeBits(altitudeBits)
	{
		for (int bx = 0; bx < blocks.x; ++bx)
		{
			for (int bz = 0; bz < blocks.y; ++bz)
			{
				lnd::LNDBlock block {};
				block.blockX = static_cast<uint32_t>(bx);
				block.blockZ = static_cast<uint32_t>(bz);
				block.mapX = static_cast<float>(bx * k_CellsPerBlock) * LandIslandInterface::k_CellSize;
				block.mapZ = static_cast<float>(bz * k_CellsPerBlock) * LandIslandInterface::k_CellSize;
				_landBlocks.emplace_back().SetLndBlock(block);
			}
		}
		for (int x = 0; x <= blocks.x * k_CellsPerBlock; ++x)
		{
			for (int z = 0; z <= blocks.y * k_CellsPerBlock; ++z)
			{
				SetCell({x, z}, cellAt(x, z));
			}
		}
		std::uniform_int_distribution<uint32_t> material(0, 7);
		std::uniform_int_distribution<uint32_t> byte(0, 255);
		_countries.resize(3);
		for (auto& c : _countries)
		{
			for (auto& entry : c.materials)
			{
				entry.indices = {material(random), material(random)};
				entry.coefficient = byte(random);
			}
		}
		_noise.resize(256 * 256);
		for (int x = 0; x < 256; ++x)
		{
			for (int z = 0; z < 256; ++z)
			{
				_noise[static_cast<size_t>(x) * 256 + static_cast<size_t>(z)] = noiseAt(x, z);
			}
		}
	}

	/// The corner in its own block and, on a border, in the 17th row or column of the blocks before it
	void SetCell(glm::ivec2 corner, const lnd::LNDCell& cell)
	{
		for (int dx = 0; dx <= 1; ++dx)
		{
			for (int dz = 0; dz <= 1; ++dz)
			{
				if ((dx == 1 && corner.x % k_CellsPerBlock != 0) || (dz == 1 && corner.y % k_CellsPerBlock != 0))
				{
					continue;
				}
				const glm::ivec2 block = corner / k_CellsPerBlock - glm::ivec2(dx, dz);
				if (block.x < 0 || block.y < 0 || block.x >= _blocks.x || block.y >= _blocks.y)
				{
					continue;
				}
				const glm::ivec2 local = corner - block * k_CellsPerBlock;
				_landBlocks[static_cast<size_t>(block.x * _blocks.y + block.y)].GetLndBlock()->cells.at(
				    static_cast<size_t>(local.x) * k_Side + static_cast<size_t>(local.y)) = cell;
			}
		}
	}

	void SetNoise(glm::ivec2 at, uint8_t value)
	{
		_noise.at(static_cast<size_t>(at.x) * 256 + static_cast<size_t>(at.y)) = value;
	}

	[[nodiscard]] uint8_t GetAltitudeBits() const final { return _altitudeBits; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2& coordinates) const final
	{
		static const lnd::LNDCell k_Empty {};
		const glm::ivec2 block = glm::ivec2(coordinates) / k_CellsPerBlock;
		if (block.x >= _blocks.x || block.y >= _blocks.y)
		{
			return k_Empty;
		}
		const glm::ivec2 local = glm::ivec2(coordinates) - block * k_CellsPerBlock;
		return _landBlocks[static_cast<size_t>(block.x * _blocks.y + block.y)].GetLndBlock()->cells.at(
		    static_cast<size_t>(local.x) * k_Side + static_cast<size_t>(local.y));
	}
	[[nodiscard]] uint16_t GetCellsPerSide() const final { return _cellsPerSide; }
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { return _landBlocks; }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { return _landBlocks; }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { return _countries; }
	[[nodiscard]] const graphics::Texture2D& GetAlbedoArray() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetSmallBump() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::Texture2D& GetCellMap() const final { throw std::logic_error("no textures"); }
	[[nodiscard]] const graphics::FrameBuffer& GetStaticShadowFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const final { throw std::logic_error("no fb"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const final { return {}; }
	uint8_t GetNoise(glm::u8vec2 pos) final { return _noise.at(static_cast<size_t>(pos.x) * 256 + pos.y); }

private:
	glm::ivec2 _blocks;
	uint16_t _cellsPerSide;
	uint8_t _altitudeBits;
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
	std::vector<uint8_t> _noise;
};

/// One block's CPU side of BuildMesh: its vertices before the open sea cells collapse
struct BuiltBlock
{
	std::vector<LandVertex> vertices;
};

std::vector<LandVertex> EmptyVertices()
{
	return std::vector<LandVertex>(LandBlock::k_VertexCount, LandVertex({}, {}, {}, {}, 0, {}, 0.0f, {}));
}

/// The previous build, unchanged: every block builds its own vertices
std::vector<BuiltBlock> BuildReference(SharedMeshIsland& island)
{
	std::vector<BuiltBlock> built;
	for (auto& block : island.GetBlocks())
	{
		auto& b = built.emplace_back();
		b.vertices = EmptyVertices();
		block.BuildVertexList(b.vertices, island);
	}
	return built;
}

/// The new build, as LandIsland::Build and LandBlock::BuildMesh do it
std::vector<BuiltBlock> BuildShared(SharedMeshIsland& island, LandBlockMeshes& meshes)
{
	std::vector<BuiltBlock> built;
	auto& blocks = island.GetBlocks();
	for (size_t i = 0; i < blocks.size(); ++i)
	{
		auto* shared = meshes.Find(i);
		auto& b = built.emplace_back();
		b.vertices = EmptyVertices();
		blocks[i].BuildVertexList(b.vertices, island, shared);
	}
	return built;
}

struct Expected
{
	size_t minSharedBlocks; // blocks that take a shared mesh
	size_t maxSharedBlocks;
};

void ExpectSameAsReference(SharedMeshIsland& island, Expected expected)
{
	const auto reference = BuildReference(island);
	LandBlockMeshes meshes(island.GetBlocks(), island);
	const auto built = BuildShared(island, meshes);
	ASSERT_EQ(reference.size(), built.size());

	size_t sharedBlocks = 0;
	std::set<const SharedLandMesh*> sharedMeshes;
	for (size_t i = 0; i < built.size(); ++i)
	{
		const auto& ref = reference[i];
		const auto& b = built[i];
		ASSERT_EQ(std::memcmp(ref.vertices.data(), b.vertices.data(), sizeof(LandVertex) * LandBlock::k_VertexCount), 0)
		    << "vertices of block " << i;
		if (const auto* shared = meshes.Find(i); shared != nullptr)
		{
			++sharedBlocks;
			sharedMeshes.insert(shared);
		}
	}
	EXPECT_GE(sharedBlocks, expected.minSharedBlocks);
	EXPECT_LE(sharedBlocks, expected.maxSharedBlocks);
	// every shared mesh serves at least two blocks
	EXPECT_LE(sharedMeshes.size() * 2, sharedBlocks);
}

lnd::LNDCell FlatCell()
{
	lnd::LNDCell cell {};
	cell.altitude = 40;
	cell.blue = 10;
	cell.green = 120;
	cell.red = 200;
	cell.luminosity = 180;
	return cell;
}

/// A cell that repeats every block: random, but the same at the same place in every block
CellAt RepeatingCells(uint32_t seed, int countries)
{
	return [seed, countries](int x, int z) {
		std::mt19937 random(seed + static_cast<uint32_t>((x % k_CellsPerBlock) * 131 + (z % k_CellsPerBlock)));
		std::uniform_int_distribution<int> byte(0, 255);
		lnd::LNDCell cell {};
		cell.altitude = static_cast<uint8_t>(byte(random));
		cell.saveColor = static_cast<uint8_t>(byte(random));
		cell.properties.country = static_cast<uint8_t>(byte(random) % countries);
		cell.properties.split = static_cast<uint8_t>(byte(random) % 2);
		cell.blue = static_cast<uint8_t>(byte(random));
		cell.green = static_cast<uint8_t>(byte(random));
		cell.red = static_cast<uint8_t>(byte(random));
		cell.luminosity = static_cast<uint8_t>(byte(random));
		cell.flags = static_cast<uint8_t>(byte(random) % 4);
		return cell;
	};
}

NoiseAt RepeatingNoise()
{
	return [](int x, int z) { return static_cast<uint8_t>(((x % k_CellsPerBlock) * 7 + (z % k_CellsPerBlock) * 3) % 50); };
}
} // namespace

TEST(LandBlockShared, FlatLand)
{
	// 5 x 4 blocks on a 512-cell map: the last column and row of blocks read the empty cells past the island, so the
	// blocks fall into four kinds and only the last corner block is alone
	std::mt19937 random(21u);
	SharedMeshIsland island(
	    {5, 4}, 512, 8, [](int, int) { return FlatCell(); }, [](int, int) { return uint8_t {7}; }, random);
	ExpectSameAsReference(island, {.minSharedBlocks = 19, .maxSharedBlocks = 19});
}

TEST(LandBlockShared, FlatLandAtTheMapEdge)
{
	// the map ends with the island: the normals clamp at its last cell and the last corners are empty cells; again
	// four kinds of blocks, the last corner block alone
	std::mt19937 random(22u);
	SharedMeshIsland island(
	    {4, 4}, 64, 8, [](int, int) { return FlatCell(); }, [](int, int) { return uint8_t {7}; }, random);
	ExpectSameAsReference(island, {.minSharedBlocks = 15, .maxSharedBlocks = 15});
}

TEST(LandBlockShared, RepeatedBlocksWithAFewVariedCorners)
{
	std::mt19937 random(23u);
	SharedMeshIsland island({6, 5}, 512, 8, RepeatingCells(5u, 3), RepeatingNoise(), random);
	const auto cells = RepeatingCells(5u, 3);
	auto hill = [&](glm::ivec2 corner, uint8_t altitude) {
		auto cell = cells(corner.x, corner.y);
		cell.altitude = altitude;
		island.SetCell(corner, cell);
	};
	// inside block (1, 1)
	hill({20, 20}, 250);
	// on the border of blocks (1, 2) and (2, 2): the last column of the first, the first of the second
	hill({48, 40}, 3);
	// the corner shared by blocks (3, 3), (3, 4), (4, 3) and (4, 4)
	hill({64, 64}, 1);
	// a corner of blocks (0, 3) and (1, 3), which (0, 2) and (1, 2) read only for the normals of their last row
	hill({16, 49}, 200);
	// one more past the border, read by block (2, 0) only for the normals of its last column
	hill({49, 5}, 90);
	// only the colour of one corner
	auto colour = cells(70, 3);
	colour.blue = static_cast<uint8_t>(colour.blue ^ 0x40);
	island.SetCell({70, 3}, colour);
	// only the not-drawn flag of one corner (the vertex list keeps it; the vertex buffer collapses such cells)
	auto sea = cells(83, 70);
	sea.flags = static_cast<uint8_t>(sea.flags ^ 0x02);
	island.SetCell({83, 70}, sea);
	// only the noise of one corner of block (2, 4) (noise is read at the place modulo 256), which picks another
	// material entry
	island.SetNoise({40, 68}, 200);
	// before the edits 26 blocks share: the inner blocks, and the inner ones of each edge (the first column and row
	// read another ring, the last ones the empty cells past the island); the edits leave 8
	ExpectSameAsReference(island, {.minSharedBlocks = 6, .maxSharedBlocks = 12});
}

TEST(LandBlockShared, TenAltitudeBits)
{
	// the high altitude bits are in saveColor: the same altitude byte with another saveColor is another block
	std::mt19937 random(24u);
	SharedMeshIsland island({6, 4}, 512, 10, RepeatingCells(9u, 2), RepeatingNoise(), random);
	const auto cells = RepeatingCells(9u, 2);
	auto cell = cells(21, 7);
	cell.saveColor = static_cast<uint8_t>(cell.saveColor ^ 0x03);
	island.SetCell({21, 7}, cell);
	// 20 blocks share before the edit, 19 after
	ExpectSameAsReference(island, {.minSharedBlocks = 15, .maxSharedBlocks = 20});
}

TEST(LandBlockShared, NoTwoBlocksAlike)
{
	// random corners everywhere: nothing is shared and every block builds as before
	std::mt19937 random(25u);
	std::mt19937 cellRandom(26u);
	std::uniform_int_distribution<int> byte(0, 255);
	SharedMeshIsland island(
	    {3, 3}, 512, 8,
	    [&](int, int) {
		    lnd::LNDCell cell {};
		    cell.altitude = static_cast<uint8_t>(byte(cellRandom));
		    cell.properties.country = static_cast<uint8_t>(byte(cellRandom) % 3);
		    cell.properties.split = static_cast<uint8_t>(byte(cellRandom) % 2);
		    cell.luminosity = static_cast<uint8_t>(byte(cellRandom));
		    return cell;
	    },
	    [&](int, int) { return static_cast<uint8_t>(byte(cellRandom)); }, random);
	ExpectSameAsReference(island, {.minSharedBlocks = 0, .maxSharedBlocks = 0});
}

TEST(LandBlockShared, TheInputsSeeEveryReadCorner)
{
	// blocks 1 and 2 of a row of 4, alike, have the same inputs; one corner past block 2's last column, which only its
	// normals read, makes their inputs and their vertices differ
	std::mt19937 random(27u);
	SharedMeshIsland island(
	    {4, 1}, 512, 8, [](int, int) { return FlatCell(); }, [](int, int) { return uint8_t {7}; }, random);
	auto& blocks = island.GetBlocks();
	auto lumpy = FlatCell();
	lumpy.altitude = 100;
	island.SetCell({20, 0}, lumpy);
	island.SetCell({36, 0}, lumpy);
	EXPECT_EQ(blocks[1].MeshInputs(island), blocks[2].MeshInputs(island));
	island.SetCell({49, 0}, lumpy);
	EXPECT_NE(blocks[1].MeshInputs(island), blocks[2].MeshInputs(island));
	auto a = EmptyVertices();
	auto b = EmptyVertices();
	blocks[1].BuildVertexList(a, island);
	blocks[2].BuildVertexList(b, island);
	EXPECT_NE(std::memcmp(a.data(), b.data(), sizeof(LandVertex) * LandBlock::k_VertexCount), 0);
}
