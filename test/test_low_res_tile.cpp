/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land's low-resolution pages (low_res): the land file's page records and each block's tile place, the DXT3
// decode to ARGB 4444 as the game reads its pages back, and a block's tile rewritten from its finished texture. The
// tile rewrite is pinned to the original's own, run natively on the same pseudo-random pages and textures. The
// decode's expected words were measured on the test machine by asking DirectDraw for the same copy the game makes as
// a land opens (a DXT3 surface from the page's own description copied into a 256 x 256 ARGB 4444 texture), over
// synthetic blocks and Land1's seven pages (docs/wiki/world/terrain.md#the-low-resolution-pages).

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <filesystem>
#include <span>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LowResTile.h"

using namespace openblack;

namespace
{
struct MeasuredBlock
{
	std::array<uint8_t, 16> block;
	std::array<uint16_t, 16> texels;
};

// The measured copies: every texel's alpha nibble differs and every colour code appears
const std::array<MeasuredBlock, 10> k_Measured {{
    // first colour larger: four colours, the two between rounded to the nearest third
    {{{0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xC2, 0xA5, 0x57, 0x04, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x0AB1, 0x108B, 0x26A4, 0x3398, 0x408B, 0x56A4, 0x6398, 0x7AB1, 0x86A4, 0x9398, 0xAAB1, 0xB08B, 0xC398, 0xDAB1, 0xE08B,
       0xF6A4}}},
    {{{0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F, 0xA5, 0x87, 0x40, 0x4F, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x18F2, 0x24E0, 0x37F1, 0x45E0, 0x54E0, 0x67F1, 0x75E0, 0x88F2, 0x97F1, 0xA5E0, 0xB8F2, 0xC4E0, 0xD5E0, 0xE8F2, 0xF4E0,
       0x07F1}}},
    {{{0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x54, 0x0A, 0xEA, 0x98, 0x42, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x6E45, 0x745C, 0x8B47, 0x9749, 0xA45C, 0xBB47, 0xC749, 0xDE45, 0xEB47, 0xF749, 0x0E45, 0x145C, 0x2749, 0x3E45, 0x445C,
       0x5B47}}},
    // first colour smaller: three colours, the third their mean rounded down, the fourth black
    {{{0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x06, 0x66, 0xD5, 0x86, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x26C3, 0x38DA, 0x47C6, 0x5000, 0x68DA, 0x77C6, 0x8000, 0x96C3, 0xA7C6, 0xB000, 0xC6C3, 0xD8DA, 0xE000, 0xF6C3, 0x08DA,
       0x17C6}}},
    {{{0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F, 0x21, 0x69, 0x2C, 0x23, 0x66, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x3284, 0x46C1, 0x54A3, 0x6000, 0x76C1, 0x84A3, 0x9000, 0xA284, 0xB4A3, 0xC000, 0xD284, 0xE6C1, 0xF000, 0x0284, 0x16C1,
       0x24A3}}},
    {{{0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0x32, 0x06, 0x1C, 0x3A, 0x63, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x4183, 0x566D, 0x6378, 0x7000, 0x866D, 0x9378, 0xA000, 0xB183, 0xC378, 0xD000, 0xE183, 0xF66D, 0x0000, 0x1183, 0x266D,
       0x3378}}},
    // equal colours count as the smaller case: the fourth is black
    {{{0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0x10, 0xF2, 0x51, 0xF2, 0x51, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x2539, 0x3539, 0x4539, 0x5000, 0x6539, 0x7539, 0x8000, 0x9539, 0xA539, 0xB000, 0xC539, 0xD539, 0xE000, 0xF539, 0x0539,
       0x1539}}},
    // the ends of a channel: 31 and 0 in red, 63 and 0 in green
    {{{0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE, 0xE5, 0xFF, 0x05, 0x00, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x0FF2, 0x1002, 0x2AA2, 0x3552, 0x4002, 0x5AA2, 0x6552, 0x7FF2, 0x8AA2, 0x9552, 0xAFF2, 0xB002, 0xC552, 0xDFF2, 0xE002,
       0xFAA2}}},
    {{{0x0F, 0x21, 0x43, 0x65, 0x87, 0xA9, 0xCB, 0xED, 0x05, 0x00, 0xE5, 0xFF, 0xE4, 0x39, 0x4E, 0x93}},
     {{0xF002, 0x0FF2, 0x1772, 0x2000, 0x3FF2, 0x4772, 0x5000, 0x6002, 0x7772, 0x8000, 0x9002, 0xAFF2, 0xB000, 0xC002, 0xDFF2,
       0xE772}}},
    {{{0x65, 0x87, 0xA9, 0xCB, 0xED, 0x0F, 0x21, 0x43, 0xBF, 0x84, 0xA0, 0x84, 0xE4, 0x39, 0x4E, 0x93}},
     {{0x589F, 0x6890, 0x789A, 0x8895, 0x9890, 0xA89A, 0xB895, 0xC89F, 0xD89A, 0xE895, 0xF89F, 0x0890, 0x1895, 0x289F, 0x3890,
       0x489A}}},
}};

// FNV-1a over the words' bytes, low byte first
uint32_t Fnv1a(const std::vector<uint16_t>& words)
{
	uint32_t hash = 0x811C9DC5u;
	for (const auto word : words)
	{
		for (const uint32_t byte : {word & 0xFFu, static_cast<uint32_t>(word >> 8)})
		{
			hash = (hash ^ byte) * 0x01000193u;
		}
	}
	return hash;
}

// A page record of width x height: a surface description naming DXT3, then the bits
lnd::LNDLowResolutionTexture MakeRecord(uint32_t width, uint32_t height, uint32_t fourCC, size_t bitsSize)
{
	lnd::LNDLowResolutionTexture record {};
	record.texels.assign(lnd::LNDLowResolutionTexture::k_SurfaceDescriptionSize + bitsSize, 0);
	const auto put = [&record](size_t offset, uint32_t value) { std::memcpy(&record.texels[offset], &value, 4); };
	put(0, 124);
	put(4, 0x81007);
	put(8, height);
	put(12, width);
	put(72, 32);
	put(76, 4);
	put(80, fourCC);
	record.header.size = static_cast<uint32_t>(record.texels.size() + 4);
	return record;
}

std::filesystem::path Land1Path()
{
	if (const char* env = std::getenv("OPENBLACK_LAND1_LND"))
	{
		return env;
	}
	if (const char* game = std::getenv("OPENBLACK_GAME_PATH"); game != nullptr && *game != '\0')
	{
		return std::filesystem::path(game) / "Data" / "Landscape" / "Land1.lnd";
	}
	return {};
}
} // namespace

TEST(LowResTile, Dxt3BlocksAsMeasured)
{
	for (size_t i = 0; i < k_Measured.size(); ++i)
	{
		const auto texels = low_res::DecodeDxt3Block(k_Measured[i].block);
		for (size_t t = 0; t < texels.size(); ++t)
		{
			EXPECT_EQ(texels[t], k_Measured[i].texels[t]) << "block " << i << " texel " << t;
		}
	}
}

TEST(LowResTile, Dxt3SurfaceRowByRow)
{
	// two blocks across, one down: the second block's texels start at column 4 of each row
	std::vector<uint8_t> bits;
	bits.insert(bits.end(), k_Measured[0].block.begin(), k_Measured[0].block.end());
	bits.insert(bits.end(), k_Measured[3].block.begin(), k_Measured[3].block.end());
	const auto texels = low_res::DecodeDxt3(bits, 8, 4);
	ASSERT_EQ(texels.size(), 32u);
	for (size_t row = 0; row < 4; ++row)
	{
		for (size_t column = 0; column < 4; ++column)
		{
			EXPECT_EQ(texels[row * 8 + column], k_Measured[0].texels[row * 4 + column]);
			EXPECT_EQ(texels[row * 8 + 4 + column], k_Measured[3].texels[row * 4 + column]);
		}
	}
	// short bits or a side that is no multiple of 4 give nothing
	EXPECT_TRUE(low_res::DecodeDxt3(std::span(bits).first(31), 8, 4).empty());
	EXPECT_TRUE(low_res::DecodeDxt3(bits, 6, 4).empty());
}

TEST(LowResTile, PageFromItsRecord)
{
	auto record = MakeRecord(4, 4, lnd::LNDLowResolutionTexture::k_FourCCDxt3, 16);
	std::memcpy(&record.texels[lnd::LNDLowResolutionTexture::k_SurfaceDescriptionSize], k_Measured[6].block.data(), 16);
	EXPECT_EQ(record.Width(), 4u);
	EXPECT_EQ(record.Height(), 4u);
	EXPECT_EQ(record.CompressedTexels().size(), 16u);
	const auto page = low_res::DecodePage(record);
	ASSERT_TRUE(page.has_value());
	EXPECT_EQ(page->width, 4u);
	EXPECT_EQ(page->height, 4u);
	EXPECT_TRUE(std::equal(page->texels.begin(), page->texels.end(), k_Measured[6].texels.begin()));

	// another compression, or a description cut short, gives no page
	EXPECT_FALSE(low_res::DecodePage(MakeRecord(4, 4, 0x31545844, 16)).has_value());
	lnd::LNDLowResolutionTexture shortRecord {};
	shortRecord.texels.resize(100);
	EXPECT_EQ(shortRecord.FourCC(), 0u);
	EXPECT_TRUE(shortRecord.CompressedTexels().empty());
	EXPECT_FALSE(low_res::DecodePage(shortRecord).has_value());
}

TEST(LowResTile, LandFileKeepsThePageAndTheTilePlace)
{
	// a land of one block and one 4 x 4 page; the block's tile place is read as whole texels
	const auto record = MakeRecord(4, 4, lnd::LNDLowResolutionTexture::k_FourCCDxt3, 16);
	lnd::LNDHeader header {};
	header.blockCount = 2;
	header.blockSize = sizeof(lnd::LNDBlock);
	header.materialSize = sizeof(lnd::LNDMaterial);
	header.countrySize = sizeof(lnd::LNDCountry);
	header.lowResolutionCount = 1;
	lnd::LNDLowResolutionTextureHeader recordHeader {};
	recordHeader.tileCount = 1;
	recordHeader.index = 3;
	recordHeader.size = record.header.size;
	lnd::LNDBlock block {};
	block.lowResTexture = 3;
	block.fuLrs = 0.25f;
	block.fvLrs = 0.75f;
	block.iuLrs = 64;
	block.ivLrs = 192;

	std::vector<uint8_t> buffer;
	const auto append = [&buffer](const void* data, size_t size) {
		const auto* bytes = static_cast<const uint8_t*>(data);
		buffer.insert(buffer.end(), bytes, bytes + size);
	};
	append(&header, sizeof(header));
	append(&recordHeader, sizeof(recordHeader));
	append(record.texels.data(), record.texels.size());
	append(&block, sizeof(block));
	buffer.resize(buffer.size() + sizeof(lnd::LNDExtraTextures), 0);

	lnd::LNDFile file;
	ASSERT_EQ(file.Open(buffer), lnd::LNDResult::Success);
	ASSERT_EQ(file.GetLowResolutionTextures().size(), 1u);
	const auto& page = file.GetLowResolutionTextures()[0];
	EXPECT_EQ(page.header.tileCount, 1u);
	EXPECT_EQ(page.header.index, 3u);
	EXPECT_EQ(page.Width(), 4u);
	EXPECT_EQ(page.FourCC(), lnd::LNDLowResolutionTexture::k_FourCCDxt3);
	EXPECT_EQ(page.CompressedTexels().size(), 16u);
	ASSERT_EQ(file.GetBlocks().size(), 1u);
	EXPECT_EQ(file.GetBlocks()[0].lowResTexture, 3u);
	EXPECT_EQ(file.GetBlocks()[0].iuLrs, 64);
	EXPECT_EQ(file.GetBlocks()[0].ivLrs, 192);
}

// No synthetic twin: the page hashes were measured from Land1.lnd's own pages, and the tile places are that file's
// data; the decode and the records are pinned on synthetic data by the tests above
TEST(LowResTile, Land1PagesAsMeasured)
{
	const auto path = Land1Path();
	if (!std::filesystem::exists(path))
	{
		GTEST_SKIP() << "no Land1.lnd at " << path.string();
	}
	lnd::LNDFile file;
	ASSERT_EQ(file.Open(path), lnd::LNDResult::Success);
	const auto& records = file.GetLowResolutionTextures();
	ASSERT_EQ(records.size(), 7u);
	// each page's words as DirectDraw gave them back, hashed
	const std::array<uint32_t, 7> measured {0x9EE128FC, 0x2F609465, 0x566EA66C, 0x9968536F, 0xA7FEA63D, 0xAF58DDFC, 0xEA04C09D};
	for (size_t i = 0; i < records.size(); ++i)
	{
		EXPECT_EQ(records[i].header.index, i);
		const auto page = low_res::DecodePage(records[i]);
		ASSERT_TRUE(page.has_value()) << "page " << i;
		EXPECT_EQ(page->width, low_res::k_PageSize);
		EXPECT_EQ(page->height, low_res::k_PageSize);
		EXPECT_EQ(Fnv1a(page->texels), measured[i]) << "page " << i;
	}
	// every block's tile is a 64 x 64 place inside its page, its UV origin the same place over 256
	for (const auto& block : file.GetBlocks())
	{
		EXPECT_LT(block.lowResTexture, records.size()) << "block " << block.index;
		EXPECT_EQ(block.iuLrs % 64, 0) << "block " << block.index;
		EXPECT_EQ(block.ivLrs % 64, 0) << "block " << block.index;
		EXPECT_GE(block.iuLrs, 0);
		EXPECT_LT(block.iuLrs, 256);
		EXPECT_GE(block.ivLrs, 0);
		EXPECT_LT(block.ivLrs, 256);
		EXPECT_FLOAT_EQ(block.fuLrs * 256.0f, static_cast<float>(block.iuLrs));
		EXPECT_FLOAT_EQ(block.fvLrs * 256.0f, static_cast<float>(block.ivLrs));
	}
}

namespace
{
// Pseudo-random ARGB 4444 words for the tile tests: state = state * 1664525 + 1013904223, a word the top 16 bits
std::vector<uint16_t> Words(uint32_t seed, size_t count)
{
	std::vector<uint16_t> words(count);
	uint32_t state = seed;
	for (auto& word : words)
	{
		state = state * 1664525u + 1013904223u;
		word = static_cast<uint16_t>(state >> 16);
	}
	return words;
}

struct TileCase
{
	bool lowResolution;
	std::vector<uint32_t> pageSeeds;
	uint32_t blockSeed;
	size_t page;
	low_res::TilePlace place;
	// the neighbours that were rewritten: which side, their page and their place
	struct Neighbour
	{
		int side; // 0 lower z, 1 higher z, 2 lower x, 3 higher x
		size_t page;
		low_res::TilePlace place;
	};
	std::vector<Neighbour> neighbours;
	// each page's FNV-1a hash after the original's rewrite
	std::vector<uint32_t> expected;
};

std::vector<std::vector<uint16_t>> RunTileCase(const TileCase& c)
{
	const uint32_t side = c.lowResolution ? 128 : 256;
	std::vector<std::vector<uint16_t>> pages;
	pages.reserve(c.pageSeeds.size());
	for (const auto seed : c.pageSeeds)
	{
		pages.push_back(Words(seed, static_cast<size_t>(side) * side));
	}
	const auto block = Words(c.blockSeed, static_cast<size_t>(side) * side);
	low_res::TileNeighbours neighbours;
	for (const auto& n : c.neighbours)
	{
		const low_res::NeighbourTile tile {.page = pages[n.page], .place = n.place};
		switch (n.side)
		{
		case 0:
			neighbours.lowerZ = tile;
			break;
		case 1:
			neighbours.higherZ = tile;
			break;
		case 2:
			neighbours.lowerX = tile;
			break;
		default:
			neighbours.higherX = tile;
			break;
		}
	}
	EXPECT_TRUE(low_res::UpdateTile(pages[c.page], side, c.place, block, side, c.lowResolution, neighbours));
	return pages;
}

// The original's rewrite, run on the same words: one block alone, four rewritten neighbours on its page, neighbours
// on another page and one not rewritten yet (left out), a block at the map's edge, and the low-resolution option
const std::array<TileCase, 5> k_TileCases {{
    {false, {11}, 21, 0, {64, 128}, {}, {0xE04DCA10}},
    {false, {12}, 22, 0, {64, 64}, {{0, 0, {0, 0}}, {1, 0, {192, 0}}, {2, 0, {0, 192}}, {3, 0, {192, 192}}}, {0x63D5F3C4}},
    {false, {13, 14}, 23, 0, {128, 0}, {{0, 1, {64, 64}}, {2, 1, {192, 128}}}, {0x216447E0, 0x618071BB}},
    {false, {15}, 24, 0, {192, 192}, {{0, 0, {128, 192}}, {3, 0, {64, 0}}}, {0x4E8CE8F3}},
    {true,
     {16, 17},
     25,
     0,
     {64, 128},
     {{0, 0, {0, 0}}, {1, 1, {192, 64}}, {2, 0, {128, 192}}, {3, 1, {0, 192}}},
     {0xA1B992FD, 0xCB9F5803}},
}};
} // namespace

TEST(LowResTile, TileRewriteAsTheOriginal)
{
	for (size_t i = 0; i < k_TileCases.size(); ++i)
	{
		const auto pages = RunTileCase(k_TileCases[i]);
		ASSERT_EQ(pages.size(), k_TileCases[i].expected.size());
		for (size_t p = 0; p < pages.size(); ++p)
		{
			EXPECT_EQ(Fnv1a(pages[p]), k_TileCases[i].expected[p]) << "case " << i << " page " << p;
		}
	}
}

TEST(LowResTile, TileBordersAsTheOriginal)
{
	// four neighbours on the block's page: a few of the words the original left there
	const auto pages = RunTileCase(k_TileCases[1]);
	const auto& page = pages[0];
	const auto at = [&page](size_t row, size_t column) { return page[row * 256 + column]; };
	// the block's tile at column 64, row 64: its corner is joined last along x, the inside comes from the texture
	EXPECT_EQ(at(64, 64), 0x69A9);
	EXPECT_EQ(at(64, 65), 0x6568);
	EXPECT_EQ(at(65, 65), 0x6568);
	EXPECT_EQ(at(127, 127), 0x671A);
	EXPECT_EQ(at(96, 126), 0x7728);
	// the border with the block before it along z: both sides hold the same mean, two texels each
	EXPECT_EQ(at(69, 64), 0xDC36);
	EXPECT_EQ(at(5, 63), 0xDC36);
	EXPECT_EQ(at(5, 62), 0xDC36);

	// with the low-resolution option a neighbour on another page is written there, at its place halved
	const auto lowPages = RunTileCase(k_TileCases[4]);
	EXPECT_EQ(lowPages[1][(32 + 3) * 128 + 96], 0x4D75);
}

TEST(LowResTile, TileOutsideItsPageWritesNothing)
{
	std::vector<uint16_t> page(256 * 256, 0x1234);
	const auto block = Words(1, 256 * 256);
	EXPECT_FALSE(low_res::UpdateTile(page, 256, {.column = 0, .row = 224}, block, 256, false, {}));
	EXPECT_FALSE(low_res::UpdateTile(page, 256, {.column = 0, .row = -64}, block, 256, false, {}));
	EXPECT_FALSE(low_res::UpdateTile(page, 256, {.column = 0, .row = 0}, std::span(block).first(1000), 256, false, {}));
	EXPECT_TRUE(std::ranges::all_of(page, [](uint16_t texel) { return texel == 0x1234; }));
}
