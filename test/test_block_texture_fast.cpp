/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The island block texture (block_texture::BuildIslandBlockTexture), which paints each distinct block once, copies it
// for the blocks with the same cells and paints along the rows, gives byte for byte the texels of the plain build kept
// below as the reference (every block painted on its own, column by column): on synthetic islands only, a flat one, one
// with a few varied cells, neighbouring blocks that differ only in the row or column they share with the next block,
// random countries and materials, 8 and 10 altitude bits, fewer texels per block than 256, places without a block,
// blocks outside the extent and two blocks at the same place. The build into a buffer that was not cleared first
// writes every texel.

#include <cstdint>

#include <algorithm>
#include <array>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

#include <LNDFile.h>
#include <glm/common.hpp>
#include <gtest/gtest.h>

#include "3D/BlockTexture.h"
#include "3D/CoastAlpha.h"
#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Graphics/Argb4444.h"
#include "Graphics/Mesh.h"

using namespace openblack;

// The plain build as it was before the faster one, kept unchanged as the reference (only its names differ)
namespace openblack::block_texture::reference
{
namespace
{
constexpr uint8_t k_OpenSeaFlag = 0x02;
} // namespace

void ReferenceBuildBlock(std::span<const lnd::LNDCell> cells, const Sources& sources, const Target& target,
                         const CellBox& box = {})
{
	if (cells.size() < static_cast<size_t>(k_CellsPerSide) * k_CellsPerSide || target.texelsPerBlock == 0)
	{
		return;
	}
	constexpr size_t k_MapSize = static_cast<size_t>(coast::k_TexelsPerBlock) * coast::k_TexelsPerBlock;
	const auto& countries = sources.countries;
	const auto& coneWeights = coast::GetConeWeights();
	const auto countryOf = [&countries](const lnd::LNDCell& cell) -> const lnd::LNDCountry& {
		return countries[std::min<size_t>(cell.properties.country, countries.size() - 1)];
	};
	const auto texels = TexelsOf(box, target.texelsPerBlock);
	for (int tx = texels.min.x; tx <= texels.max.x; ++tx)
	{
		// the original's texel under this one (the same one with 256 texels per block)
		const int ox = tx * coast::k_TexelsPerBlock / target.texelsPerBlock;
		const int cx = ox / coast::k_TexelsPerCell;
		const int i = ox % coast::k_TexelsPerCell;
		for (int tz = texels.min.y; tz <= texels.max.y; ++tz)
		{
			const int oz = tz * coast::k_TexelsPerBlock / target.texelsPerBlock;
			const int cz = oz / coast::k_TexelsPerCell;
			const int j = oz % coast::k_TexelsPerCell;
			// the block's own 17 x 17 cells: +1 along z, +17 along x; corners (x, z), (x, z + 1), (x + 1, z + 1),
			// (x + 1, z) like the weights
			const auto base = static_cast<size_t>(cx) * k_CellsPerSide + static_cast<size_t>(cz);
			const std::array<const lnd::LNDCell*, 4> corners = {
			    &cells[base], &cells[base + 1], &cells[base + k_CellsPerSide + 1], &cells[base + k_CellsPerSide]};
			uint16_t texel = 0;
			if ((corners[0]->flags & k_OpenSeaFlag) == 0 && !countries.empty())
			{
				const auto& weights = coneWeights.at(static_cast<size_t>(i * coast::k_TexelsPerCell + j));
				int32_t h = 0;
				for (size_t k = 0; k < corners.size(); ++k)
				{
					h += static_cast<int32_t>(weights.at(k)) * corners.at(k)->Altitude(sources.altitudeBits);
				}
				const size_t index = static_cast<size_t>(ox) * coast::k_TexelsPerBlock + static_cast<size_t>(oz);
				const uint8_t noise = sources.noise.size() >= k_MapSize ? sources.noise[index] : uint8_t {0};
				const uint8_t bump = sources.bump.size() >= k_MapSize ? sources.bump[index] : uint8_t {0x80};
				// one build when the four corners share the country, else one per corner country blended by
				// BlendCorners (the same result for equal ones, the weights add up to 255)
				const auto own = corners[0]->properties.country;
				const bool single = std::all_of(corners.begin(), corners.end(),
				                                [own](const lnd::LNDCell* c) { return c->properties.country == own; });
				if (single)
				{
					texel = CountryTexel(countryOf(*corners[0]), h, noise, bump, sources.materials, index);
				}
				else
				{
					std::array<uint16_t, 4> painted {};
					for (size_t k = 0; k < corners.size(); ++k)
					{
						painted.at(k) = CountryTexel(countryOf(*corners.at(k)), h, noise, bump, sources.materials, index);
					}
					texel = BlendCorners(painted, weights);
				}
			}
			const auto row = static_cast<size_t>(target.origin.y + tz);
			const auto column = static_cast<size_t>(target.origin.x + tx);
			std::ranges::copy(graphics::argb4444::Unpack(texel),
			                  target.rgba.subspan((row * target.width + column) * 4, 4).begin());
		}
	}
}

std::vector<uint8_t> BuildIslandBlockTexture(const LandIslandInterface& island, glm::u16vec2 extentIndexMin,
                                             glm::u16vec2 indexSize, uint16_t texelsPerBlock, std::span<const uint8_t> noise,
                                             std::span<const uint8_t> bump, const Materials& materials)
{
	const size_t width = static_cast<size_t>(indexSize.x) * texelsPerBlock;
	const size_t height = static_cast<size_t>(indexSize.y) * texelsPerBlock;
	std::vector<uint8_t> texels(width * height * 4, 0);
	const Sources sources {
	    .countries = island.GetCountries(),
	    .materials = materials,
	    .noise = noise,
	    .bump = bump,
	    .altitudeBits = island.GetAltitudeBits(),
	};
	for (const auto& block : island.GetBlocks())
	{
		const auto* cells = block.GetCells();
		const auto position = block.GetBlockPosition() - glm::ivec2(extentIndexMin);
		if (cells == nullptr || position.x < 0 || position.y < 0 || position.x >= indexSize.x || position.y >= indexSize.y)
		{
			continue;
		}
		const Target target {
		    .rgba = texels,
		    .width = width,
		    .origin = position * static_cast<int>(texelsPerBlock),
		    .texelsPerBlock = texelsPerBlock,
		};
		ReferenceBuildBlock(std::span(cells, static_cast<size_t>(k_CellsPerSide) * k_CellsPerSide), sources, target);
	}
	return texels;
}
} // namespace openblack::block_texture::reference

namespace
{
constexpr int k_Side = block_texture::k_CellsPerSide;
constexpr size_t k_MapTexels = 256 * 256;

/// Blocks given one by one, each with its own 17 x 17 cells, so that two neighbours need not agree on the cells they
/// share
class BlocksIsland final: public LandIslandInterface
{
public:
	BlocksIsland(uint8_t altitudeBits, std::vector<lnd::LNDCountry> countries)
	    : _altitudeBits(altitudeBits)
	    , _countries(std::move(countries))
	{
	}

	void Add(glm::ivec2 position, const std::array<lnd::LNDCell, 17 * 17>& cells)
	{
		lnd::LNDBlock block {};
		block.blockX = static_cast<uint32_t>(position.x);
		block.blockZ = static_cast<uint32_t>(position.y);
		block.cells = cells;
		_landBlocks.emplace_back().SetLndBlock(block);
	}

	[[nodiscard]] uint8_t GetAltitudeBits() const final { return _altitudeBits; }
	[[nodiscard]] float GetHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] float GetUnflattenedHeightAt(glm::vec2) const final { return 0.0f; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { throw std::logic_error("no cells"); }
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
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

private:
	uint8_t _altitudeBits;
	std::vector<LandBlock> _landBlocks;
	std::vector<lnd::LNDCountry> _countries;
};

using Cells = std::array<lnd::LNDCell, 17 * 17>;

void SetAltitude(lnd::LNDCell& cell, uint16_t altitude, uint8_t altitudeBits)
{
	cell.altitude = static_cast<uint8_t>(altitude & 0xFFu);
	if (altitudeBits > 8)
	{
		const auto highMask = static_cast<uint8_t>(0xFFu >> (16u - altitudeBits));
		cell.saveColor = static_cast<uint8_t>((cell.saveColor & ~highMask) | ((altitude >> 8u) & highMask));
	}
}

lnd::LNDCell& At(Cells& cells, int x, int z)
{
	return cells.at(static_cast<size_t>(x) * k_Side + static_cast<size_t>(z));
}

/// Every cell at the same altitude and in the same country, none of open sea
Cells FlatCells(uint16_t altitude, uint8_t country, uint8_t altitudeBits)
{
	Cells cells {};
	for (auto& cell : cells)
	{
		SetAltitude(cell, altitude, altitudeBits);
		cell.properties.country = country;
	}
	return cells;
}

/// The blocks of a grid of blocks.x x blocks.y cut from one grid of corners, so that neighbours share their border
/// cells as in a land file
std::vector<Cells> CutGrid(glm::ivec2 blocks, const std::vector<lnd::LNDCell>& grid)
{
	const glm::ivec2 corners = blocks * block_texture::k_CellsPerBlock + 1;
	std::vector<Cells> out;
	for (int bx = 0; bx < blocks.x; ++bx)
	{
		for (int bz = 0; bz < blocks.y; ++bz)
		{
			Cells& cells = out.emplace_back();
			for (int x = 0; x < k_Side; ++x)
			{
				for (int z = 0; z < k_Side; ++z)
				{
					const auto gx = static_cast<size_t>(bx * block_texture::k_CellsPerBlock + x);
					const auto gz = static_cast<size_t>(bz * block_texture::k_CellsPerBlock + z);
					At(cells, x, z) = grid[gx * static_cast<size_t>(corners.y) + gz];
				}
			}
		}
	}
	return out;
}

struct Maps
{
	std::vector<uint16_t> materials;
	std::vector<uint8_t> noise;
	std::vector<uint8_t> bump;
};

Maps RandomMaps(std::mt19937& random, size_t materialCount)
{
	Maps maps {std::vector<uint16_t>(materialCount * k_MapTexels), std::vector<uint8_t>(k_MapTexels),
	           std::vector<uint8_t>(k_MapTexels)};
	std::uniform_int_distribution<int> word(0, 0xFFFF);
	std::uniform_int_distribution<int> byte(0, 0xFF);
	std::ranges::generate(maps.materials, [&] { return static_cast<uint16_t>(word(random)); });
	std::ranges::generate(maps.noise, [&] { return static_cast<uint8_t>(byte(random)); });
	std::ranges::generate(maps.bump, [&] { return static_cast<uint8_t>(byte(random)); });
	return maps;
}

/// `count` countries over `materialCount` materials, with one and two material entries
std::vector<lnd::LNDCountry> RandomCountries(std::mt19937& random, size_t count, uint32_t materialCount)
{
	std::uniform_int_distribution<uint32_t> material(0, materialCount - 1);
	std::uniform_int_distribution<uint32_t> coefficient(0, 256);
	std::uniform_int_distribution<int> percent(0, 99);
	std::vector<lnd::LNDCountry> countries(count);
	for (auto& country : countries)
	{
		for (auto& entry : country.materials)
		{
			const auto first = material(random);
			entry.indices = {first, percent(random) < 30 ? first : material(random)};
			entry.coefficient = coefficient(random);
		}
	}
	return countries;
}

struct Build
{
	glm::u16vec2 extentMin {0, 0};
	glm::u16vec2 indexSize {1, 1};
	uint16_t texelsPerBlock {256};
};

/// The fast build, into a fresh vector and into a buffer full of garbage, against the reference, byte for byte
void ExpectSameAsReference(const BlocksIsland& island, const Maps& maps, size_t materialCount, const Build& build)
{
	const block_texture::Materials materials {maps.materials, materialCount};
	const auto expected = block_texture::reference::BuildIslandBlockTexture(
	    island, build.extentMin, build.indexSize, build.texelsPerBlock, maps.noise, maps.bump, materials);
	const auto fast = block_texture::BuildIslandBlockTexture(island, build.extentMin, build.indexSize, build.texelsPerBlock,
	                                                         maps.noise, maps.bump, materials);
	std::vector<uint8_t> unclear(expected.size(), uint8_t {0xCD});
	block_texture::BuildIslandBlockTexture(island, build.extentMin, build.indexSize, build.texelsPerBlock, maps.noise,
	                                       maps.bump, materials, unclear);
	ASSERT_EQ(fast.size(), expected.size());
	const auto differing = [&expected](const std::vector<uint8_t>& image) {
		size_t count = 0;
		for (size_t t = 0; t < expected.size(); ++t)
		{
			count += image[t] != expected[t] ? 1 : 0;
		}
		return count;
	};
	EXPECT_EQ(differing(fast), 0u) << build.texelsPerBlock << " texels per block";
	EXPECT_EQ(differing(unclear), 0u) << build.texelsPerBlock << " texels per block, buffer not cleared";
	// the image is not all clear, or the comparison would prove little
	EXPECT_TRUE(std::ranges::any_of(expected, [](uint8_t b) { return b != 0; }));
}
} // namespace

TEST(BlockTextureFast, FlatLandIsTheSame)
{
	std::mt19937 random(11);
	const auto maps = RandomMaps(random, 3);
	for (const uint16_t texelsPerBlock : {uint16_t {256}, uint16_t {64}, uint16_t {48}})
	{
		BlocksIsland island(8, RandomCountries(random, 2, 3));
		const glm::ivec2 blocks {4, 3};
		const auto grid = FlatCells(32, 1, 8);
		for (int x = 0; x < blocks.x; ++x)
		{
			for (int z = 0; z < blocks.y; ++z)
			{
				island.Add({5 + x, 7 + z}, grid);
			}
		}
		ExpectSameAsReference(
		    island, maps, 3,
		    {.extentMin = glm::u16vec2(5, 7), .indexSize = glm::u16vec2(blocks), .texelsPerBlock = texelsPerBlock});
	}
}

TEST(BlockTextureFast, AFewVariedCellsAreTheSame)
{
	std::mt19937 random(23);
	const auto maps = RandomMaps(random, 4);
	for (const uint8_t altitudeBits : {uint8_t {8}, uint8_t {10}})
	{
		BlocksIsland island(altitudeBits, RandomCountries(random, 3, 4));
		const glm::ivec2 blocks {4, 3};
		const glm::ivec2 corners = blocks * block_texture::k_CellsPerBlock + 1;
		std::vector<lnd::LNDCell> grid(static_cast<size_t>(corners.x) * static_cast<size_t>(corners.y));
		for (auto& cell : grid)
		{
			SetAltitude(cell, 40, altitudeBits);
			cell.properties.country = 0;
		}
		const auto cell = [&grid, &corners](int x, int z) -> lnd::LNDCell& {
			return grid[static_cast<size_t>(x) * static_cast<size_t>(corners.y) + static_cast<size_t>(z)];
		};
		// a hill, a cell of another country, a cell of open sea, a corner on a block border and one on the island's
		// edge
		SetAltitude(cell(5, 6), altitudeBits > 8 ? uint16_t {700} : uint16_t {200}, altitudeBits);
		cell(20, 9).properties.country = 2;
		cell(40, 30).flags = 0x02;
		SetAltitude(cell(16, 32), 0, altitudeBits);
		cell(64, 48).properties.country = 1;
		const auto cut = CutGrid(blocks, grid);
		for (int bx = 0; bx < blocks.x; ++bx)
		{
			for (int bz = 0; bz < blocks.y; ++bz)
			{
				island.Add({bx, bz}, cut[static_cast<size_t>(bx * blocks.y + bz)]);
			}
		}
		ExpectSameAsReference(island, maps, 4,
		                      {.extentMin = glm::u16vec2(0, 0), .indexSize = glm::u16vec2(blocks), .texelsPerBlock = 256});
		ExpectSameAsReference(island, maps, 4,
		                      {.extentMin = glm::u16vec2(0, 0), .indexSize = glm::u16vec2(blocks), .texelsPerBlock = 37});
	}
}

TEST(BlockTextureFast, NeighboursThatDifferOnlyAtAnEdgeAreTheSame)
{
	std::mt19937 random(37);
	const auto maps = RandomMaps(random, 3);
	BlocksIsland island(8, RandomCountries(random, 3, 3));
	const auto base = FlatCells(60, 0, 8);
	// the same block, then with only its last column (the next block's first, along x) changed, then only its last
	// row (along z), then only its far corner: each a different texture along that edge
	auto column = base;
	auto row = base;
	auto corner = base;
	for (int i = 0; i < k_Side; ++i)
	{
		SetAltitude(At(column, 16, i), 90, 8);
		At(row, i, 16).properties.country = 2;
	}
	At(corner, 16, 16).properties.country = 1;
	island.Add({0, 0}, base);
	island.Add({1, 0}, column);
	island.Add({2, 0}, base);
	island.Add({0, 1}, row);
	island.Add({1, 1}, corner);
	island.Add({2, 1}, column);
	island.Add({0, 2}, corner);
	island.Add({1, 2}, row);
	island.Add({2, 2}, base);
	ExpectSameAsReference(island, maps, 3,
	                      {.extentMin = glm::u16vec2(0, 0), .indexSize = glm::u16vec2(3, 3), .texelsPerBlock = 256});
	ExpectSameAsReference(island, maps, 3,
	                      {.extentMin = glm::u16vec2(0, 0), .indexSize = glm::u16vec2(3, 3), .texelsPerBlock = 64});
}

TEST(BlockTextureFast, RandomCountriesAndMaterialsAreTheSame)
{
	std::mt19937 random(53);
	for (const size_t materialCount : {size_t {1}, size_t {4}, size_t {6}})
	{
		const auto maps = RandomMaps(random, materialCount);
		for (const uint8_t altitudeBits : {uint8_t {8}, uint8_t {10}})
		{
			BlocksIsland island(altitudeBits, RandomCountries(random, 5, static_cast<uint32_t>(materialCount)));
			std::uniform_int_distribution<int> altitude(0, (1 << altitudeBits) - 1);
			std::uniform_int_distribution<int> country(0, 6); // past the last country too
			std::uniform_int_distribution<int> percent(0, 99);
			// one random block, repeated on most places, and a few random others
			Cells repeated {};
			const auto randomise = [&](Cells& cells) {
				for (auto& cell : cells)
				{
					SetAltitude(cell, static_cast<uint16_t>(altitude(random)), altitudeBits);
					cell.properties.country = static_cast<uint8_t>(country(random));
					cell.flags = percent(random) < 5 ? uint8_t {0x02} : uint8_t {0};
				}
			};
			randomise(repeated);
			for (int x = 0; x < 3; ++x)
			{
				for (int z = 0; z < 2; ++z)
				{
					if ((x + z) % 3 == 1)
					{
						Cells other {};
						randomise(other);
						island.Add({x, z}, other);
					}
					else
					{
						island.Add({x, z}, repeated);
					}
				}
			}
			ExpectSameAsReference(island, maps, materialCount,
			                      {.extentMin = glm::u16vec2(0, 0), .indexSize = glm::u16vec2(3, 2), .texelsPerBlock = 64});
		}
	}
}

TEST(BlockTextureFast, MissingOutsideAndRepeatedPlacesAreTheSame)
{
	std::mt19937 random(71);
	const auto maps = RandomMaps(random, 2);
	BlocksIsland island(8, RandomCountries(random, 2, 2));
	const auto low = FlatCells(30, 0, 8);
	const auto high = FlatCells(120, 1, 8);
	// the extent starts at block (1, 1). (2, 1) is painted with `low` and then with `high` over it; later blocks with
	// `low` must not copy the place that `high` covered. (3, 2) has no block, (6, 6) and (0, 1) lie outside the extent
	island.Add({2, 1}, low);
	island.Add({2, 1}, high);
	island.Add({1, 1}, low);
	island.Add({6, 6}, high);
	island.Add({0, 1}, low);
	island.Add({1, 2}, high);
	island.Add({2, 2}, low);
	island.Add({1, 2}, low);
	island.Add({3, 1}, high);
	ExpectSameAsReference(island, maps, 2,
	                      {.extentMin = glm::u16vec2(1, 1), .indexSize = glm::u16vec2(3, 2), .texelsPerBlock = 32});
	// an island without countries is left clear everywhere
	BlocksIsland bare(8, {});
	bare.Add({0, 0}, low);
	const block_texture::Materials materials {maps.materials, 2};
	const auto expected = block_texture::reference::BuildIslandBlockTexture(bare, glm::u16vec2(0, 0), glm::u16vec2(2, 1), 16,
	                                                                        maps.noise, maps.bump, materials);
	std::vector<uint8_t> unclear(expected.size(), uint8_t {0xCD});
	block_texture::BuildIslandBlockTexture(bare, glm::u16vec2(0, 0), glm::u16vec2(2, 1), 16, maps.noise, maps.bump, materials,
	                                       unclear);
	EXPECT_EQ(unclear, expected);
}
