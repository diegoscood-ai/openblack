/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footprints printed into a block's texture (3D/BlockPrints.h), pinned to the original's own print run natively by
// the exe oracle on synthetic footprints and textures: each case's texture after the print, by its hash, the texels it
// changed and some of them (docs/wiki/world/terrain.md#the-prints-the-16th-step).

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "3D/BlockPrints.h"

using namespace openblack::block_texture;

namespace
{
// the same generator the oracle's cases were filled with
std::vector<uint16_t> Lcg(uint32_t seed, size_t count)
{
	std::vector<uint16_t> out(count);
	uint32_t x = seed;
	for (auto& value : out)
	{
		x = x * 1103515245u + 12345u;
		value = static_cast<uint16_t>(x >> 16);
	}
	return out;
}

uint64_t Fnv(const std::vector<uint16_t>& words)
{
	uint64_t hash = 0xCBF29CE484222325ull;
	for (const auto word : words)
	{
		for (const auto byte : {static_cast<uint8_t>(word & 0xFF), static_cast<uint8_t>(word >> 8)})
		{
			hash ^= byte;
			hash *= 0x100000001B3ull;
		}
	}
	return hash;
}

// the object's matrix as the oracle's cases made it, in single floats: turned by `angle` about y and scaled
std::array<float, 12> Placement(double angle, double scale, float x, float z)
{
	const auto c = static_cast<float>(std::cos(angle) * scale);
	const auto s = static_cast<float>(std::sin(angle) * scale);
	return {c, 0.0f, -s, 0.0f, 1.0f, 0.0f, s, 0.0f, c, x, 0.0f, z};
}

std::vector<PrintTriangle> Square(float half, float width, float height, float pad = 0.0f)
{
	const glm::vec2 a {-half, -half};
	const glm::vec2 b {half, -half};
	const glm::vec2 c {half, half};
	const glm::vec2 d {-half, half};
	const glm::vec2 ua {-pad, -pad};
	const glm::vec2 ub {width + pad, -pad};
	const glm::vec2 uc {width + pad, height + pad};
	const glm::vec2 ud {-pad, height + pad};
	return {{.corners = {a, b, c}, .texels = {ua, ub, uc}}, {.corners = {a, c, d}, .texels = {ua, uc, ud}}};
}

struct Case
{
	std::string_view name;
	int32_t width;
	int32_t height;
	std::vector<PrintTriangle> triangles;
	std::array<float, 12> placement;
	glm::uvec2 block;
	PrintMode mode;
	uint32_t strength;
	int32_t size;
	uint32_t pictureSeed;
	uint32_t textureSeed;
	uint64_t hash;
	size_t changed;
	std::vector<std::pair<size_t, uint16_t>> picks;
};

std::vector<Case> Cases()
{
	const std::vector<PrintTriangle> thin {
	    {.corners = {glm::vec2 {-50.0f, -1.0f}, glm::vec2 {50.0f, 0.5f}, glm::vec2 {49.0f, 2.0f}},
	     .texels = {glm::vec2 {0.0f, 0.0f}, glm::vec2 {16.0f, 2.0f}, glm::vec2 {15.0f, 5.0f}}}};
	const std::vector<PrintTriangle> point {{.corners = {glm::vec2 {5.0f}, glm::vec2 {5.0f}, glm::vec2 {5.0f}},
	                                         .texels = {glm::vec2 {1.0f}, glm::vec2 {2.0f}, glm::vec2 {3.0f}}}};
	const auto river = Placement(0.9, 1.0, 880.0f, 1050.0f);
	return {
	    {"centre",
	     16,
	     16,
	     Square(20.0f, 16, 16),
	     Placement(0.0, 1.0, 80.0f, 80.0f),
	     {0, 0},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     1,
	     2,
	     0xB1C1FE5CBA52C339ull,
	     3970,
	     {{24672, 0x3378}, {27283, 0x409E}, {30080, 0xF824}, {32906, 0xC4A1}, {35723, 0x2511}, {38525, 0x726D}}},
	    {"turned",
	     32,
	     16,
	     Square(30.0f, 32, 16),
	     Placement(0.5, 1.0, 1680.0f, 1650.0f),
	     {10, 10},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     3,
	     4,
	     0xBD40A337C2FE4051ull,
	     8537,
	     {{16189, 0xC442}, {24947, 0xF102}, {28810, 0x120C}, {32543, 0xF4A7}, {36160, 0xB271}, {40001, 0x6A79}}},
	    {"scaled_over_edge",
	     16,
	     32,
	     Square(25.0f, 16, 32),
	     Placement(-1.1, 2.0, 165.0f, 10.0f),
	     {1, 0},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     5,
	     6,
	     0x42FB23BC39946E27ull,
	     8114,
	     {{0, 0x8510}, {3176, 0xB819}, {7172, 0xCA4A}, {11278, 0xB52D}, {15882, 0xF9C5}, {20799, 0x4484}}},
	    {"outside_uv_clamped",
	     8,
	     8,
	     Square(40.0f, 8, 8, 3.5f),
	     Placement(2.3, 0.75, 400.0f, 560.0f),
	     {2, 3},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     7,
	     8,
	     0x55B84E24D7D692CCull,
	     8890,
	     {{15484, 0xEA4E}, {25749, 0x2C13}, {29815, 0x4847}, {32896, 0xE305}, {35964, 0xC879}, {40071, 0x6A4A}}},
	    {"half_texels",
	     16,
	     16,
	     Square(10.0f, 16, 16),
	     Placement(0.0, 1.0, 80.3125f, 80.3125f),
	     {0, 0},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     9,
	     10,
	     0xF015D67AE0A3C48Full,
	     996,
	     {{28784, 0x6CC3}, {30086, 0x374D}, {31610, 0x6617}, {32903, 0x98ED}, {34192, 0x7403}, {35719, 0xD4CE}}},
	    {"thin_triangle",
	     16,
	     16,
	     thin,
	     Placement(0.2, 1.0, 80.0f, 80.0f),
	     {0, 0},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     11,
	     12,
	     0x7D7EB4295C617797ull,
	     356,
	     {{12686, 0xF374}, {23431, 0x2616}, {31107, 0xF9A9}, {38782, 0xC671}, {43898, 0xA9C8}, {48503, 0xA687}}},
	    {"alpha_only",
	     16,
	     16,
	     Square(30.0f, 16, 16),
	     river,
	     {5, 6},
	     PrintMode::Alpha,
	     0x100,
	     256,
	     13,
	     14,
	     0x6E706B73D3FB208Dull,
	     4328,
	     {{15767, 0x5D36}, {26259, 0x409E}, {30130, 0x0FC8}, {33134, 0x1DBA}, {36480, 0x386F}, {40864, 0x34BD}}},
	    {"alpha_only_min",
	     16,
	     16,
	     Square(30.0f, 16, 16),
	     river,
	     {5, 6},
	     PrintMode::Alpha,
	     0xFF,
	     256,
	     13,
	     14,
	     0x6E706B73D3FB208Dull,
	     4328,
	     {{15767, 0x5D36}, {26259, 0x409E}, {30130, 0x0FC8}, {33134, 0x1DBA}, {36480, 0x386F}, {40864, 0x34BD}}},
	    {"alpha_only_strength",
	     16,
	     16,
	     Square(30.0f, 16, 16),
	     river,
	     {5, 6},
	     PrintMode::Alpha,
	     0x47,
	     256,
	     13,
	     14,
	     0xAD98BDF5AF48687Dull,
	     2384,
	     {{15768, 0x9A65}, {26518, 0x8491}, {30132, 0x4BF1}, {33170, 0x59F8}, {36475, 0x7F92}, {41093, 0x6C56}}},
	    {"low_res",
	     16,
	     16,
	     Square(30.0f, 16, 16),
	     Placement(0.4, 1.0, 1700.0f, 1720.0f),
	     {10, 10},
	     PrintMode::Colour,
	     0xFF,
	     128,
	     15,
	     16,
	     0x357CF9C9EBC202CAull,
	     2169,
	     {{6355, 0x498D}, {8421, 0x9B64}, {9418, 0x8294}, {10326, 0x4AD8}, {11228, 0x0272}, {12260, 0x9229}}},
	    {"degenerate",
	     16,
	     16,
	     point,
	     Placement(0.0, 1.0, 80.0f, 80.0f),
	     {0, 0},
	     PrintMode::Colour,
	     0xFF,
	     256,
	     17,
	     18,
	     0x772C41397764452Cull,
	     1,
	     {{34952, 0x1747}}},
	};
}
} // namespace

TEST(BlockPrints, FootprintsPrintAsTheOriginal)
{
	for (const auto& c : Cases())
	{
		const auto pictureTexels = Lcg(c.pictureSeed, static_cast<size_t>(c.width * c.height));
		const auto before = Lcg(c.textureSeed, static_cast<size_t>(c.size * c.size));
		auto texels = before;
		const PrintPicture picture {.width = c.width, .height = c.height, .texels = pictureTexels, .triangles = c.triangles};
		PrintFootprint(texels, c.size, c.block, picture, c.placement, c.mode, c.strength);
		size_t changed = 0;
		for (size_t i = 0; i < texels.size(); ++i)
		{
			changed += texels[i] != before[i] ? 1 : 0;
		}
		EXPECT_EQ(changed, c.changed) << c.name;
		EXPECT_EQ(Fnv(texels), c.hash) << c.name;
		for (const auto& [at, value] : c.picks)
		{
			EXPECT_EQ(texels.at(at), value) << c.name << " texel " << at;
		}
	}
}

TEST(BlockPrints, AColourPrintKeepsTheLandsAlphaAndNeverCoversItWholly)
{
	// one opaque white texel over black land: 15 x 240 / 255 = 14 a nibble
	const std::array<uint16_t, 1> white {0xFFFF};
	const std::vector<PrintTriangle> cover = Square(400.0f, 1, 1);
	std::vector<uint16_t> texels(256 * 256, 0x5000);
	PrintFootprint(texels, 256, {0, 0}, {.width = 1, .height = 1, .texels = white, .triangles = cover},
	               Placement(0.0, 1.0, 80.0f, 80.0f), PrintMode::Colour);
	EXPECT_EQ(texels[0], 0x5EEE);
	EXPECT_EQ(texels[256 * 256 - 1], 0x5EEE);
}

TEST(BlockPrints, AnAlphaPrintOnlyLowersTheAlpha)
{
	const std::array<uint16_t, 1> clear {0x3123};
	const std::vector<PrintTriangle> cover = Square(400.0f, 1, 1);
	std::vector<uint16_t> texels(256 * 256, 0xC456);
	PrintFootprint(texels, 256, {0, 0}, {.width = 1, .height = 1, .texels = clear, .triangles = cover},
	               Placement(0.0, 1.0, 80.0f, 80.0f), PrintMode::Alpha);
	EXPECT_EQ(texels[1000], 0x3456);
	// raised by the strength's high nibble first: 3 + 4 = 7
	std::ranges::fill(texels, uint16_t {0xC456});
	PrintFootprint(texels, 256, {0, 0}, {.width = 1, .height = 1, .texels = clear, .triangles = cover},
	               Placement(0.0, 1.0, 80.0f, 80.0f), PrintMode::Alpha, 0x4F);
	EXPECT_EQ(texels[1000], 0x7456);
}

TEST(BlockPrints, TheShadowTableAsTheOriginalBuildsIt)
{
	// the original's table read back after it built it (exe oracle), by the samples covered: 0 to 8 of 8
	const auto table = ShadowTable(false);
	const auto low = ShadowTable(true);
	constexpr std::array<uint8_t, 9> k_ByCount {0, 31, 63, 95, 127, 159, 191, 223, 255};
	constexpr std::array<uint8_t, 9> k_LowByCount {0, 15, 31, 47, 63, 79, 95, 111, 127};
	for (uint32_t i = 0; i < 256; ++i)
	{
		const auto covered = static_cast<size_t>(std::popcount(i));
		EXPECT_EQ(table.at(i), k_ByCount.at(covered)) << i;
		EXPECT_EQ(low.at(i), k_LowByCount.at(covered)) << i;
	}
	constexpr std::array<uint8_t, 16> k_First {0, 31, 31, 63, 31, 63, 63, 95, 31, 63, 63, 95, 63, 95, 95, 127};
	for (size_t i = 0; i < k_First.size(); ++i)
	{
		EXPECT_EQ(table.at(i), k_First.at(i)) << i;
	}
}

TEST(BlockPrints, AShadowDarkensTheColourByItsCoverage)
{
	const auto table = ShadowTable(false);
	std::array<uint16_t, 3> texels {0xFABC, 0xFABC, 0x7ABC};
	const std::array<uint8_t, 3> mask {0xFF, 0x01, 0x00};
	ApplyShadow(texels, mask, table);
	// whole: x 128 / 256; one sample of eight: x 240 / 256; none: as it was; the alpha kept
	EXPECT_EQ(texels[0], 0xF556);
	EXPECT_EQ(texels[1], 0xF9AB);
	EXPECT_EQ(texels[2], 0x7ABC);
}

TEST(BlockPrints, SnowWhitensAsTheOriginal)
{
	struct SnowCase
	{
		std::string_view name;
		glm::uvec2 block;
		int32_t size;
		uint32_t depthSeed;
		double depthScale;
		uint32_t noiseSeed;
		uint32_t textureSeed;
		uint64_t hash;
		size_t changed;
		std::vector<std::pair<size_t, uint16_t>> picks;
	};
	// the original's snow print run natively by the exe oracle, with its blend table as the snow cover builds it
	const std::vector<SnowCase> cases {
	    {"deep",
	     {5, 7},
	     256,
	     1,
	     255.0,
	     2,
	     3,
	     0xD71005110A14E3B3ull,
	     27783,
	     {{0, 0xCBBA}, {9968, 0xFEEE}, {24628, 0x6DEE}, {35523, 0x5BAE}, {46398, 0xD45E}, {57343, 0x022A}}},
	    {"shallow",
	     {5, 7},
	     256,
	     4,
	     12.0,
	     5,
	     6,
	     0x2DA87D63B55DD126ull,
	     196,
	     {{57, 0x7699}, {26815, 0xB3DE}, {37101, 0xA838}, {44217, 0x20C3}, {49118, 0xC65D}, {55373, 0x6171}}},
	    {"mixed",
	     {10, 10},
	     256,
	     7,
	     80.0,
	     8,
	     9,
	     0x08D225A0330E6198ull,
	     8361,
	     {{0, 0x4EE7}, {11018, 0x15A9}, {22375, 0xA726}, {31147, 0x8C87}, {39523, 0x6283}, {51106, 0xE7E4}}},
	    {"edge_of_map",
	     {31, 31},
	     256,
	     10,
	     200.0,
	     11,
	     12,
	     0xBFB633BEBB566802ull,
	     16708,
	     {{8, 0x6E24}, {11922, 0x1E7D}, {22703, 0x8C9A}, {35940, 0x82E8}, {44909, 0x8C8A}, {51297, 0xADAC}}},
	    {"low_res",
	     {3, 4},
	     128,
	     13,
	     120.0,
	     14,
	     15,
	     0xD6526A74923C9DA7ull,
	     3141,
	     {{4, 0xDC8C}, {2672, 0xDA6E}, {5470, 0xAB4B}, {8885, 0x81A5}, {11549, 0x1BBB}, {13827, 0xE07B}}},
	    {"none", {2, 2}, 256, 16, 4.0, 17, 18, 0x3D6CFBCEC48D428Full, 0, {}},
	};
	for (const auto& c : cases)
	{
		std::vector<float> depths;
		for (const auto word : Lcg(c.depthSeed, 128 * 128))
		{
			depths.push_back(static_cast<float>(static_cast<double>(word) / 65535.0 * c.depthScale));
		}
		std::vector<uint8_t> noise;
		std::vector<uint8_t> whites;
		for (const auto word : Lcg(c.noiseSeed, 256 * 256))
		{
			const auto n = static_cast<uint8_t>(word & 0xFF);
			noise.push_back(n);
			whites.push_back(static_cast<uint8_t>(14 + (((n >> 2) + (n >> 1)) & 1)));
		}
		const auto before = Lcg(c.textureSeed, static_cast<size_t>(c.size * c.size));
		auto texels = before;
		PrintSnow(texels, c.size, c.block, {.depths = depths, .noise = noise, .whites = whites});
		size_t changed = 0;
		for (size_t i = 0; i < texels.size(); ++i)
		{
			changed += texels[i] != before[i] ? 1 : 0;
		}
		EXPECT_EQ(changed, c.changed) << c.name;
		EXPECT_EQ(Fnv(texels), c.hash) << c.name;
		for (const auto& [at, value] : c.picks)
		{
			EXPECT_EQ(texels.at(at), value) << c.name << " texel " << at;
		}
	}
}
