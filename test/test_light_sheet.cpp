/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sheet of light raised along a recognised gesture's trail (src/Particles/LightSheet.h), against the wiki
// (docs/bw1-notes/magic.md, "The light sheet"). Synthetic points only.

#include <cmath>
#include <cstdint>

#include <bit>
#include <vector>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/LightSheet.h"

using namespace openblack::particles;

namespace
{
/// Points standing in a row along x, one every `step`
std::vector<glm::vec3> Row(int count, float step)
{
	std::vector<glm::vec3> points;
	for (int i = 0; i < count; ++i)
	{
		points.emplace_back(static_cast<float>(i) * step, 0.0f, 0.0f);
	}
	return points;
}

/// A sheet on `count` points two units apart, its colour 0x804020, fed at full strength
LightSheet FedSheet(int count, float height)
{
	LightSheet sheet;
	sheet.Start(Row(count, 2.0f), 0x804020u, height, 0.03f);
	sheet.SetStrength(1.0f);
	return sheet;
}
} // namespace

TEST(GestureTrailSheet, ItsStrengthRunsAlongItAndTheWaveRolls)
{
	LightSheet sheet;
	std::vector<glm::vec3> points;
	for (int i = 0; i < 50; ++i)
	{
		points.emplace_back(static_cast<float>(i), 0.0f, 0.0f);
	}
	sheet.Start(points, 0x0000FFu, 9.0f, 0.03f);
	sheet.SetStrength(1.0f);
	sheet.Update(0.02f);
	EXPECT_FLOAT_EQ(sheet.Strengths()[0], 0.0f);
	sheet.Update(0.02f);
	// Moved one point along after 0.03 seconds, fed in at the first
	EXPECT_FLOAT_EQ(sheet.Strengths()[0], 1.0f);
	EXPECT_FLOAT_EQ(sheet.Strengths()[1], 0.0f);
	sheet.Update(0.07f);
	EXPECT_FLOAT_EQ(sheet.Strengths()[2], 1.0f);
	EXPECT_NEAR(sheet.Heights()[0], static_cast<float>((std::cos(-0.11 * 3.0) * 0.3 + 0.8) * 9.0), 1e-4f);

	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 150u);
	EXPECT_EQ(triangles.size(), 49u * 12u);
	// Dark at the land and the top, its light a quarter of the way up
	EXPECT_EQ(vertices[0].argb, 0xFF000000u);
	EXPECT_EQ(vertices[2].argb, 0xFF000000u);
	EXPECT_EQ(vertices[1].argb & 0xFFu, 0xFFu * 255u >> 8u);
	EXPECT_EQ(vertices[1].specularArgb, (((vertices[1].argb & 0xFFFFFFu) & 0xFEFEFEu) >> 1u) | 0x20000000u);
	EXPECT_NEAR(vertices[1].position.y, (vertices[2].position.y - vertices[0].position.y) * 0.25f, 1e-4f);
	// Its top spread out from the middle
	EXPECT_NEAR(vertices[2].position.x, ((0.0f - 24.5f) * 1.1f) + 24.5f, 1e-4f);
}

TEST(LightSheet, FewerThanTwoPointsBuildNothing)
{
	for (const int count : {0, 1})
	{
		auto sheet = FedSheet(count, 9.0f);
		// No wave without a second point: the heights stay as they started, nothing is divided by zero
		sheet.Update(0.05f);
		for (const float height : sheet.Heights())
		{
			EXPECT_EQ(height, 0.0f);
		}
		std::vector<LightSheet::Vertex> vertices(1);
		std::vector<uint32_t> triangles(1);
		sheet.Build(vertices, triangles);
		EXPECT_TRUE(vertices.empty());
		EXPECT_TRUE(triangles.empty());
		// Not built, so its middle is still the unbuilt one
		EXPECT_EQ(std::bit_cast<uint32_t>(sheet.Middle().x), 0xCDCDCDCDu);
	}
	// The strength is still fed in at a lone point
	auto lone = FedSheet(1, 9.0f);
	lone.Update(0.05f);
	EXPECT_EQ(lone.Strengths()[0], 1.0f);
}

TEST(LightSheet, ItsStrengthMovesOnTheUnroundedSum)
{
	auto under = FedSheet(50, 9.0f);
	under.Update(0.0299f);
	EXPECT_EQ(under.Strengths()[0], 0.0f);

	auto over = FedSheet(50, 9.0f);
	over.Update(0.0301f);
	EXPECT_EQ(over.Strengths()[0], 1.0f);
	EXPECT_EQ(over.Strengths()[1], 0.0f);

	// 0.09 seconds at once moves it three points along
	auto three = FedSheet(50, 9.0f);
	three.Update(0.09f);
	EXPECT_EQ(three.Strengths()[2], 1.0f);
	EXPECT_EQ(three.Strengths()[3], 0.0f);
}

TEST(LightSheet, TheWaveUsesTheSinglePrecisionDepthAndMiddle)
{
	auto sheet = FedSheet(50, 9.0f);
	sheet.Update(0.0f);
	// At the first point and no time the wave is at its crest: (depth + middle) x height, from 0.3f and 0.8f
	const float expected = static_cast<float>((static_cast<double>(0.3f) + static_cast<double>(0.8f)) * 9.0);
	EXPECT_EQ(sheet.Heights()[0], expected);
	// Exact 0.3 and 0.8 would round to a different float
	EXPECT_NE(sheet.Heights()[0], static_cast<float>((0.3 + 0.8) * 9.0));
}

TEST(LightSheet, ItsCornersColoursAndTexture)
{
	auto sheet = FedSheet(4, 2.0f);
	sheet.Update(0.0301f);
	// Only the first point has been fed
	ASSERT_EQ(sheet.Strengths()[0], 1.0f);
	ASSERT_EQ(sheet.Strengths()[1], 0.0f);

	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 12u);

	// The land and the top are opaque black with nothing added
	for (size_t i = 0; i < 4; ++i)
	{
		EXPECT_EQ(vertices[i * 3].argb, 0xFF000000u);
		EXPECT_EQ(vertices[i * 3].specularArgb, 0u);
		EXPECT_EQ(vertices[(i * 3) + 2].argb, 0xFF000000u);
		EXPECT_EQ(vertices[(i * 3) + 2].specularArgb, 0u);
	}

	// The fed point's bright line: each channel times its light over 256, opaque; the added colour is half of it with
	// an alpha of 0x20
	const auto light = static_cast<uint32_t>(static_cast<double>(sheet.Heights()[0]) * 50.0);
	ASSERT_GT(light, 0u);
	ASSERT_LT(light, 255u);
	const uint32_t red = (0x80u * light) >> 8u;
	const uint32_t green = (0x40u * light) >> 8u;
	const uint32_t blue = (0x20u * light) >> 8u;
	EXPECT_EQ(vertices[1].argb, 0xFF000000u | (red << 16u) | (green << 8u) | blue);
	EXPECT_EQ(vertices[1].specularArgb, 0x20000000u | ((red >> 1u) << 16u) | ((green >> 1u) << 8u) | (blue >> 1u));
	// An unfed point has no light, but its added colour still carries the alpha
	EXPECT_EQ(vertices[4].argb, 0xFF000000u);
	EXPECT_EQ(vertices[4].specularArgb, 0x20000000u);

	// The stars repeat every ten units along the sheet; their rows are 1, 0.75 and 0 less the slide
	EXPECT_FLOAT_EQ(vertices[0].uv.x, 0.0f);
	EXPECT_FLOAT_EQ(vertices[3].uv.x, 0.2f);
	EXPECT_FLOAT_EQ(vertices[6].uv.x, 0.4f);
	EXPECT_FLOAT_EQ(vertices[9].uv.x, 0.6f);
	EXPECT_EQ(vertices[1].uv.x, vertices[0].uv.x);
	EXPECT_EQ(vertices[2].uv.x, vertices[0].uv.x);
	EXPECT_FLOAT_EQ(vertices[0].uv.y, 1.0f + 0.0301f);
	EXPECT_FLOAT_EQ(vertices[1].uv.y, 0.75f + 0.0301f);
	EXPECT_FLOAT_EQ(vertices[2].uv.y, 0.0301f);

	// Four triangles between each pair of points, in the game's order
	const std::vector<uint32_t> expected {
	    0, 1, 3, 1, 4, 3, 1, 2, 4, 2, 5, 4, 3, 4, 6, 4, 7, 6, 4, 5, 7, 5, 8, 7, 6, 7, 9, 7, 10, 9, 7, 8, 10, 8, 11, 10,
	};
	EXPECT_EQ(triangles, expected);
}

TEST(LightSheet, TheStarsSlideWrapsAtOne)
{
	auto sheet = FedSheet(2, 2.0f);
	sheet.Update(1.25f);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 6u);
	EXPECT_EQ(vertices[0].uv.y, 1.25f);
	EXPECT_EQ(vertices[1].uv.y, 1.0f);
	EXPECT_EQ(vertices[2].uv.y, 0.25f);
	sheet.Update(0.5f);
	sheet.Build(vertices, triangles);
	EXPECT_EQ(vertices[2].uv.y, 0.75f);
}

TEST(LightSheet, ItsMiddleAndSpread)
{
	LightSheet sheet;
	sheet.Start({{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {4.0f, 0.0f, 0.0f}, {6.0f, 3.0f, 9.0f}}, 0xFFFFFFu, 1.0f, 0.03f);
	// Before its first build each axis of the middle is the unbuilt pattern, far below everything
	EXPECT_EQ(std::bit_cast<uint32_t>(sheet.Middle().x), 0xCDCDCDCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(sheet.Middle().y), 0xCDCDCDCDu);
	EXPECT_EQ(std::bit_cast<uint32_t>(sheet.Middle().z), 0xCDCDCDCDu);
	EXPECT_EQ(sheet.Middle().x, -431602080.0f);
	EXPECT_EQ(sheet.Spread(), 1.1f);

	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	// The mean of its points
	EXPECT_EQ(sheet.Middle().x, 3.0f);
	EXPECT_EQ(sheet.Middle().y, 0.75f);
	EXPECT_EQ(sheet.Middle().z, 2.25f);
	// Nothing fed, so the top is the point spread out from the middle by 1.1
	EXPECT_FLOAT_EQ(vertices[2].position.x, ((0.0f - 3.0f) * 1.1f) + 3.0f);
	EXPECT_FLOAT_EQ(vertices[2].position.z, ((0.0f - 2.25f) * 1.1f) + 2.25f);

	// A sheet of its own spread; starting it again brings back the default
	sheet.SetSpread(1.0f);
	sheet.Build(vertices, triangles);
	EXPECT_EQ(vertices[2].position.x, vertices[0].position.x);
	EXPECT_EQ(vertices[2].position.y, vertices[0].position.y);
	EXPECT_EQ(vertices[2].position.z, vertices[0].position.z);
	sheet.Start({{0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}}, 0xFFFFFFu, 1.0f, 0.03f);
	EXPECT_EQ(sheet.Spread(), 1.1f);
}
