/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature's lean onto the slope, pinned to the original's bits: every placement below was produced by running the
// original game's own code natively, with a made-up land whose height is a x + b z + c x z + d. The heights it gives at
// the points sampled are exact in float, so the fake below gives the same ones.

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureLean.h"

using namespace openblack;

namespace
{
struct LeanCase
{
	const char* name;
	glm::vec3 position;
	uint32_t cosine;
	uint32_t sine;
	float scale;
	float radius;
	bool carried;
	std::array<float, 4> land;          // the slope along x, along z, the x z term and the height at the origin
	std::array<uint32_t, 12> placement; // the axes x, y and z, then the position
};

// The land flat, sloping along x or z, along both with the body turned and scaled, steep enough to reach both limits,
// curved, the body carried, a body of no radius on flat land, and a lean just at the limit
constexpr std::array k_Cases = {
    // clang-format off
    LeanCase {"flat", {1000.5f, 20.0f, 1500.25f}, 0x3f800000, 0x00000000, 1.0f, 3.25f, false, {0.0f, 0.0f, 0.0f, 20.0f}, {0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"slopeX", {1000.5f, 20.0f, 1500.25f}, 0x3f800000, 0x00000000, 1.0f, 3.25f, false, {0.125f, 0.0f, 0.0f, 0.0f}, {0x3f800000, 0x3d99999a, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"slopeZ", {1000.5f, 20.0f, 1500.25f}, 0x3f800000, 0x00000000, 1.0f, 3.25f, false, {0.0f, -0.0625f, 0.0f, 0.0f}, {0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, 0xbd19999a, 0x3f800000, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"both_turned", {1000.5f, 20.0f, 1500.25f}, 0x3f43ccb3, 0x3f24eb73, 2.5f, 3.25f, false, {0.125f, 0.25f, 0.0f, 0.0f}, {0x3ff4bfe0, 0x3ec51d59, 0x3fce2650, 0x00000000, 0x40200000, 0x00000000, 0xbfce2650, 0x3e2a0276, 0x3ff4bfe0, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"clampX", {1000.5f, 20.0f, 1500.25f}, 0x3e88f59d, 0x3f76abc0, 1.0f, 3.25f, false, {1.0f, -2.0f, 0.0f, 0.0f}, {0x3e88f59d, 0xbe55d456, 0x3f76abc0, 0x00000000, 0x3f800000, 0x00000000, 0xbf76abc0, 0xbebd1723, 0x3e88f59d, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"quad", {100.5f, 20.0f, 150.25f}, 0xbf013d97, 0xbf5cfb4b, 1.75f, 4.5f, false, {0.03125f, -0.015625f, 0.001953125f, 3.0f}, {0xbf622bc8, 0xbeabf767, 0xbfc15be2, 0x00000000, 0x3fe00000, 0x00000000, 0x3fc15be2, 0x3e4b4d3a, 0xbf622bc8, 0x42c90000, 0x41a00000, 0x43164000}},
    LeanCase {"held", {1000.5f, 20.0f, 1500.25f}, 0x3f43ccb3, 0x3f24eb73, 2.5f, 3.25f, true, {0.125f, 0.25f, 0.0f, 0.0f}, {0x3ff4bfe0, 0x00000000, 0x3fce2650, 0x00000000, 0x40200000, 0x00000000, 0xbfce2650, 0x00000000, 0x3ff4bfe0, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"d0_flat", {1000.5f, 20.0f, 1500.25f}, 0x3f800000, 0x00000000, 1.0f, 0.0f, false, {0.0f, 0.0f, 0.0f, 20.0f}, {0x3f800000, 0xbe99999a, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, 0xbe99999a, 0x3f800000, 0x447a2000, 0x41a00000, 0x44bb8800}},
    LeanCase {"at03", {1000.5f, 20.0f, 1500.25f}, 0x3f800000, 0x00000000, 1.0f, 2.5f, false, {0.5f, 0.0f, 0.0f, 0.0f}, {0x3f800000, 0x3e99999a, 0x00000000, 0x00000000, 0x3f800000, 0x00000000, 0x00000000, 0x00000000, 0x3f800000, 0x447a2000, 0x41a00000, 0x44bb8800}},
    // clang-format on
};

/// The rotation about y the original builds from the cosine and sine of the body's angle
glm::mat3 TurnedBy(uint32_t cosine, uint32_t sine)
{
	const float c = std::bit_cast<float>(cosine);
	const float s = std::bit_cast<float>(sine);
	return {glm::vec3(c, 0.0f, s), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(std::bit_cast<float>(sine ^ 0x80000000u), 0.0f, c)};
}

std::array<uint32_t, 12> BitsOf(const glm::mat4& model)
{
	std::array<uint32_t, 12> bits {};
	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 3; ++row)
		{
			bits.at(static_cast<size_t>((column * 3) + row)) = std::bit_cast<uint32_t>(model[column][row]);
		}
	}
	return bits;
}
} // namespace

TEST(CreatureLeanOracle, PlacementMatchesTheOriginal)
{
	for (const auto& c : k_Cases)
	{
		SCOPED_TRACE(c.name);
		const auto ground = [&c](glm::vec2 point) {
			return (c.land[0] * point.x) + (c.land[1] * point.y) + (c.land[2] * point.x * point.y) + c.land[3];
		};
		const auto heights = creature_lean::Sample(c.position, c.radius, c.carried, ground);
		const auto lean = creature_lean::FromHeights(heights, c.radius);
		const auto model = creature_lean::Model(c.position, TurnedBy(c.cosine, c.sine), glm::vec3(c.scale), lean);
		EXPECT_EQ(BitsOf(model), c.placement);
	}
}

TEST(CreatureLean, SamplesTheLandAroundTheMiddleAlongTheWorldAxes)
{
	std::vector<glm::vec2> asked;
	const auto heights = creature_lean::Sample(glm::vec3(10.0f, 5.0f, 20.0f), 2.0f, false, [&asked](glm::vec2 point) {
		asked.push_back(point);
		return point.x + (100.0f * point.y);
	});
	EXPECT_EQ(asked, (std::vector {glm::vec2(12.0f, 20.0f), glm::vec2(8.0f, 20.0f), glm::vec2(10.0f, 22.0f),
	                               glm::vec2(10.0f, 18.0f)}));
	EXPECT_EQ(heights.xAhead, 2012.0f);
	EXPECT_EQ(heights.xBehind, 2008.0f);
	EXPECT_EQ(heights.zAhead, 2210.0f);
	EXPECT_EQ(heights.zBehind, 1810.0f);
}

TEST(CreatureLean, ACarriedCreatureDoesNotAskTheLand)
{
	bool asked = false;
	const auto heights = creature_lean::Sample(glm::vec3(10.0f, 5.0f, 20.0f), 2.0f, true, [&asked](glm::vec2) {
		asked = true;
		return 100.0f;
	});
	EXPECT_FALSE(asked);
	EXPECT_EQ(heights.xAhead, 5.0f);
	EXPECT_EQ(heights.zBehind, 5.0f);
	const auto lean = creature_lean::FromHeights(heights, 2.0f);
	EXPECT_EQ(lean.x, 0.0f);
	EXPECT_EQ(lean.z, 0.0f);
}

TEST(CreatureLean, KeptWithinTheLimitBothWays)
{
	const auto steep = creature_lean::FromHeights({.xAhead = 50.0f, .xBehind = 0.0f, .zAhead = 0.0f, .zBehind = 50.0f}, 1.0f);
	EXPECT_EQ(steep.x, creature_lean::k_Limit);
	EXPECT_EQ(steep.z, -creature_lean::k_Limit);
	const auto gentle = creature_lean::FromHeights({.xAhead = 1.0f, .xBehind = 0.0f, .zAhead = 0.0f, .zBehind = 1.0f}, 5.0f);
	EXPECT_EQ(gentle.x, 1.0f * 0.6f / 10.0f);
	EXPECT_EQ(gentle.z, -1.0f * 0.6f / 10.0f);
}

TEST(CreatureLean, ABodyOfNoRadiusOnFlatLandLeansTheLowest)
{
	const auto lean = creature_lean::FromHeights({.xAhead = 3.0f, .xBehind = 3.0f, .zAhead = 3.0f, .zBehind = 3.0f}, 0.0f);
	EXPECT_EQ(lean.x, -creature_lean::k_Limit);
	EXPECT_EQ(lean.z, -creature_lean::k_Limit);
}

TEST(CreatureLean, NoLeanLeavesThePlacementUpright)
{
	const glm::mat3 rotation {glm::vec3(0.6f, 0.0f, 0.8f), glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(-0.8f, 0.0f, 0.6f)};
	const auto model = creature_lean::Model(glm::vec3(1.0f, 2.0f, 3.0f), rotation, glm::vec3(2.0f), {});
	EXPECT_EQ(model[0], glm::vec4(1.2f, 0.0f, 1.6f, 0.0f));
	EXPECT_EQ(model[1], glm::vec4(0.0f, 2.0f, 0.0f, 0.0f));
	EXPECT_EQ(model[2], glm::vec4(-1.6f, 0.0f, 1.2f, 0.0f));
	EXPECT_EQ(model[3], glm::vec4(1.0f, 2.0f, 3.0f, 1.0f));
}
