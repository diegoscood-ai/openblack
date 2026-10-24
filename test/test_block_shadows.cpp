/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The static shadows' raster (3D/BlockShadows.h), pinned to the original's own run natively by the exe oracle on
// triangle sets drawn from the generator below: each mask by its hash and its lit samples; and the projection along
// the sun, pinned to the original's bits
// (docs/wiki/rendering/shadows-and-reflections.md#shadows-three-systems-in-the-original).

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <span>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "3D/BlockShadows.h"

using namespace openblack::block_texture;

namespace
{
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

uint64_t Fnv(std::span<const uint8_t> bytes)
{
	uint64_t hash = 0xCBF29CE484222325ull;
	for (const auto byte : bytes)
	{
		hash ^= byte;
		hash *= 0x100000001B3ull;
	}
	return hash;
}

float F(uint32_t bits)
{
	return std::bit_cast<float>(bits);
}

uint32_t B(float value)
{
	return std::bit_cast<uint32_t>(value);
}

// as the oracle's cases: each triangle a centre in the ranges and three corners within +-spread of it, from the
// generator's words / 65536, each corner rounded to a single float
struct Triangles
{
	std::vector<glm::vec2> points;
	std::vector<std::array<uint16_t, 3>> triangles;
};

Triangles Make(uint32_t seed, size_t count, double x0, double x1, double y0, double y1, double spread)
{
	const auto words = Lcg(seed, count * 8);
	Triangles out;
	for (size_t t = 0; t < count; ++t)
	{
		std::array<double, 8> u {};
		for (size_t k = 0; k < u.size(); ++k)
		{
			u.at(k) = static_cast<double>(words[t * 8 + k]) / 65536.0;
		}
		const double cx = x0 + u[0] * (x1 - x0);
		const double cy = y0 + u[1] * (y1 - y0);
		for (size_t k = 0; k < 3; ++k)
		{
			out.points.emplace_back(static_cast<float>(cx + (u.at(2 + 2 * k) * 2.0 - 1.0) * spread),
			                        static_cast<float>(cy + (u.at(3 + 2 * k) * 2.0 - 1.0) * spread));
		}
		const auto first = static_cast<uint16_t>(3 * t);
		out.triangles.push_back({first, static_cast<uint16_t>(first + 1), static_cast<uint16_t>(first + 2)});
	}
	return out;
}
} // namespace

TEST(BlockShadows, TrianglesFillTheMaskAsTheOriginal)
{
	struct Case
	{
		std::string_view name;
		uint32_t seed;
		size_t count;
		double x0, x1, y0, y1, spread;
		int32_t size;
		bool doubleSided;
		bool temple;
		uint32_t oddRowsOnly;
		uint64_t hash;
		size_t lit;
	};
	const std::array<Case, 7> cases {{
	    {"few_large", 1, 6, 0, 1024, 0, 512, 200.0, 256, false, false, 0, 0xFD8522E1312D9C97ull, 52847},
	    {"many_small", 2, 400, 0, 1024, 0, 512, 6.0, 256, false, false, 0, 0x5E81CA154F8CE01Dull, 2209},
	    {"double_sided", 3, 120, -50, 1080, -30, 540, 60.0, 256, true, false, 0, 0x89DE0CDD2DCC570Eull, 85322},
	    {"temple", 4, 120, -50, 1080, -30, 540, 60.0, 256, false, true, 0, 0x1A2F6DC442EC18CDull, 99202},
	    {"off_the_mask", 5, 80, -400, 1400, -300, 800, 300.0, 256, true, false, 0, 0xACC6304CD55F0DDDull, 427409},
	    {"odd_rows", 6, 100, 0, 1024, 0, 512, 40.0, 256, true, false, 1, 0xF263E491FB186595ull, 26490},
	    {"low_res", 7, 150, -20, 540, -20, 270, 30.0, 128, false, false, 0, 0x635779D34701B09Aull, 15782},
	}};
	for (const auto& c : cases)
	{
		const auto set = Make(c.seed, c.count, c.x0, c.x1, c.y0, c.y1, c.spread);
		std::vector<uint8_t> mask(static_cast<size_t>(c.size * c.size) + 4, 0);
		ShadowRaster raster(c.size);
		DrawShadowTriangles(raster, mask, c.size, set.points, set.triangles, c.doubleSided, c.temple, c.oddRowsOnly);
		const std::span<const uint8_t> texels(mask.data(), static_cast<size_t>(c.size * c.size));
		size_t lit = 0;
		for (const auto byte : texels)
		{
			lit += static_cast<size_t>(std::popcount(byte));
		}
		EXPECT_EQ(lit, c.lit) << c.name;
		EXPECT_EQ(Fnv(texels), c.hash) << c.name;
		EXPECT_EQ(mask[mask.size() - 4] | mask[mask.size() - 3] | mask[mask.size() - 2] | mask[mask.size() - 1], 0) << c.name;
	}
}

TEST(BlockShadows, ASampleIsItsTexelsBitByRowAndColumn)
{
	// one texel's 8 samples: columns 4 to 7 and rows 2 to 3 are texel (1, 1)'s, its bits 0..3 the even row, 4..7 the
	// odd one
	std::vector<uint8_t> mask(8 * 8 + 4, 0);
	ShadowRaster raster(8);
	const std::vector<glm::vec2> points {{4.0f, 2.0f}, {8.0f, 2.0f}, {8.0f, 4.0f}, {4.0f, 4.0f}};
	const std::vector<std::array<uint16_t, 3>> square {{0, 1, 2}, {0, 2, 3}};
	DrawShadowTriangles(raster, mask, 8, points, square, true, false);
	EXPECT_EQ(mask[8 + 1], 0xFF);
	EXPECT_EQ(std::count_if(mask.begin(), mask.end(), [](uint8_t b) { return b != 0; }), 1);
}

TEST(BlockShadows, VerticesFallAlongTheSunOntoTheGround)
{
	// the original's projection (bits through its oracle-verified model), then into the mask of the block at
	// (1120, 800)
	const std::array<float, 12> matrix {F(0x3F4CCCCD), 0.0f, F(0xBF19999A), 0.0f,          F(0x3FC00000), 0.0f,
	                                    F(0x3F19999A), 0.0f, F(0x3F4CCCCD), F(0x449A5000), F(0x42150000), F(0x4476F000)};
	const std::vector<glm::vec3> exact {{0.0f, 0.0f, 0.0f},
	                                    {10.0f, 20.0f, -5.0f},
	                                    {-7.5f, 3.25f, 12.0f},
	                                    {2.0f, -40.0f, 1.0f},
	                                    {static_cast<float>(33.3), static_cast<float>(61.7), static_cast<float>(-18.9)}};
	const auto ground = ProjectShadow(exact, matrix, matrix[10]);
	constexpr std::array<std::array<uint32_t, 2>, 5> k_Ground {{{0x449A5000, 0x4476F000},
	                                                            {0x449EB200, 0x447BF200},
	                                                            {0x449B1300, 0x447BB000},
	                                                            {0x449A9600, 0x4476D600},
	                                                            {0x44A7D400, 0x4482AD00}}};
	constexpr std::array<std::array<uint32_t, 2>, 5> k_Mask {{{0x44963333, 0x43B73333},
	                                                          {0x44A6399A, 0x43EF4CCD},
	                                                          {0x44A56667, 0x43C0F333},
	                                                          {0x4495E000, 0x43BAB333},
	                                                          {0x44C45333, 0x4432199A}}};
	ASSERT_EQ(ground.size(), k_Ground.size());
	for (size_t i = 0; i < ground.size(); ++i)
	{
		EXPECT_EQ(B(ground[i].x), k_Ground.at(i)[0]) << i;
		EXPECT_EQ(B(ground[i].y), k_Ground.at(i)[1]) << i;
		const auto point = ShadowMaskPoint(ground[i], {1120.0f, 800.0f}, 256);
		EXPECT_EQ(B(point.x), k_Mask.at(i)[0]) << i;
		EXPECT_EQ(B(point.y), k_Mask.at(i)[1]) << i;
	}
}

TEST(BlockShadows, ASkinnedPartFollowsItsBonesAndMayFallUnderTheGround)
{
	// the original's projection of a skinned part (its oracle-verified model): runs of 2 and 3 vertices on two bones
	const std::array<float, 12> first {F(0x3F4CCCCD), 0.0f, F(0xBF19999A), 0.0f,   1.0f,  0.0f,
	                                   F(0x3F19999A), 0.0f, F(0x3F4CCCCD), 100.0f, 12.5f, 200.0f};
	const std::array<float, 12> second {1.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 140.25f, 50.0f, 180.75f};
	const std::array<std::array<float, 12>, 2> bones {first, second};
	const std::vector<glm::vec3> vertices {
	    {1.0f, 2.0f, 3.0f}, {-4.0f, -30.0f, 5.0f}, {7.5f, 8.25f, -9.0f}, {10.0f, -60.0f, 11.0f}, {0.0f, 0.0f, 0.0f}};
	const std::array<BoneRun, 2> runs {{{.count = 2, .bone = 0}, {.count = 3, .bone = 1}}};
	const auto ground = ProjectSkinnedShadow(vertices, runs, bones, 37.25f);
	constexpr std::array<std::array<uint32_t, 2>, 5> k_Ground {{{0x429FB000, 0x43331000},
	                                                            {0x42344000, 0x4317A000},
	                                                            {0x4324A000, 0x433CA000},
	                                                            {0x43050000, 0x432E8000},
	                                                            {0x43190000, 0x43418000}}};
	ASSERT_EQ(ground.size(), k_Ground.size());
	for (size_t i = 0; i < ground.size(); ++i)
	{
		EXPECT_EQ(B(ground[i].x), k_Ground.at(i)[0]) << i;
		EXPECT_EQ(B(ground[i].y), k_Ground.at(i)[1]) << i;
	}
}

namespace
{
struct AlphaMesh
{
	std::vector<glm::vec3> positions;
	std::vector<glm::vec2> uvs;
	std::vector<std::array<uint16_t, 3>> triangles;
};

// two crossed upright cards w wide each side and h high, as the oracle's cases
AlphaMesh LeafCards(float w, float h)
{
	return {.positions = {{-w, 0, 0}, {w, 0, 0}, {w, h, 0}, {-w, h, 0}, {0, 0, -w}, {0, 0, w}, {0, h, w}, {0, h, -w}},
	        .uvs = {{0, 1}, {1, 1}, {1, 0}, {0, 0}, {0, 1}, {1, 1}, {1, 0}, {0, 0}},
	        .triangles = {{0, 1, 2}, {0, 2, 3}, {4, 5, 6}, {4, 6, 7}}};
}

// n vertices and n triangles from the generator, every value a word / 65536
AlphaMesh Random(uint32_t seed, size_t n, double spread, double height, double low = 0.0, double uvLow = 0.0,
                 double uvHigh = 1.0)
{
	const auto words = Lcg(seed, n * 8);
	AlphaMesh mesh;
	for (size_t i = 0; i < n; ++i)
	{
		std::array<double, 5> u {};
		for (size_t k = 0; k < u.size(); ++k)
		{
			u.at(k) = static_cast<double>(words[i * 8 + k]) / 65536.0;
		}
		mesh.positions.emplace_back(static_cast<float>((u[0] * 2 - 1) * spread),
		                            static_cast<float>(low + u[1] * (height - low)),
		                            static_cast<float>((u[2] * 2 - 1) * spread));
		mesh.uvs.emplace_back(static_cast<float>(uvLow + u[3] * (uvHigh - uvLow)),
		                      static_cast<float>(uvLow + u[4] * (uvHigh - uvLow)));
		mesh.triangles.push_back({static_cast<uint16_t>(words[i * 8 + 5] % n), static_cast<uint16_t>(words[i * 8 + 6] % n),
		                          static_cast<uint16_t>(words[i * 8 + 7] % n)});
	}
	return mesh;
}

std::vector<uint8_t> AlphaBitmap(uint32_t seed)
{
	constexpr std::array<uint8_t, 8> k_Alphas {0x00, 0x00, 0x10, 0x80, 0xF0, 0xF0, 0x70, 0xA0};
	std::vector<uint8_t> out;
	for (const auto word : Lcg(seed, 64 * 64))
	{
		out.push_back(k_Alphas.at(word % 8));
	}
	return out;
}

std::array<float, 12> Turned(double yaw, double scale, float x, float y, float z)
{
	const auto c = static_cast<float>(std::cos(yaw) * scale);
	const auto s = static_cast<float>(std::sin(yaw) * scale);
	return {c, 0.0f, -s, 0.0f, static_cast<float>(scale), 0.0f, s, 0.0f, c, x, y, z};
}
} // namespace

TEST(BlockShadows, ATexturesAlphaCastsItsShadowAsTheOriginal)
{
	struct Case
	{
		std::string_view name;
		AlphaMesh mesh;
		uint32_t alphaSeed;
		std::array<float, 12> placement;
		glm::uvec2 block;
		uint64_t hash;
		size_t changed;
	};
	// the original's texture shadow run natively by the exe oracle on these objects (the parts it leaves out, of
	// another level of detail, physics only or skinned, are not given)
	const std::vector<Case> cases {
	    {"leaf_cards", LeafCards(8, 20), 1, Turned(0.0, 1.0, 80.0f, 0.0f, 80.0f), {0, 0}, 0x4702B718726F4025ull, 916},
	    {"turned_scaled", LeafCards(6, 15), 2, Turned(0.7, 1.6, 50.0f, 12.0f, 90.0f), {0, 0}, 0xC94FB9166601DAB5ull, 1413},
	    {"block_offset",
	     LeafCards(9, 18),
	     1,
	     Turned(-1.2, 1.0, 1690.0f, 35.0f, 3205.0f),
	     {10, 20},
	     0x96F608F160CD7FE5ull,
	     1028},
	    {"over_the_edges", LeafCards(10, 25), 3, Turned(2.0, 1.2, 158.0f, 0.0f, 156.0f), {0, 0}, 0xF76047049B2D71E5ull, 125},
	    {"mesh", Random(4, 40, 15.0, 30.0), 2, Turned(1.1, 1.0, 80.0f, 5.0f, 70.0f), {0, 0}, 0xDC8D00D57D9FEB05ull, 2131},
	    {"under_ground_and_wrapped",
	     Random(5, 30, 14.0, 20.0, -10.0, -1.5, 2.5),
	     3,
	     Turned(0.4, 1.0, 90.0f, -10.0f, 70.0f),
	     {0, 0},
	     0x27502E37C27C4F25ull,
	     1317},
	    {"parts_left_out", LeafCards(5, 10), 2, Turned(0.0, 1.0, 80.0f, 0.0f, 80.0f), {0, 0}, 0x9575BD636776E255ull, 329},
	};
	for (const auto& c : cases)
	{
		const auto alpha = AlphaBitmap(c.alphaSeed);
		const std::array<TextureShadowPart, 1> parts {
		    {{.positions = c.mesh.positions, .uvs = c.mesh.uvs, .triangles = c.mesh.triangles, .alpha = alpha}}};
		std::vector<uint8_t> mask(256 * 256, 0);
		DrawTextureShadow(mask, 256, c.block, c.placement, parts);
		EXPECT_EQ(Fnv(mask), c.hash) << c.name;
		EXPECT_EQ(static_cast<size_t>(std::ranges::count_if(mask, [](uint8_t b) { return b != 0; })), c.changed) << c.name;
	}
}
