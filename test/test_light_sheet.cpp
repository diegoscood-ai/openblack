/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The sheet of light raised along a recognised gesture's trail (src/Particles/LightSheet.h), against the wiki
// (docs/wiki/magic/hand-casting.md#the-light-sheet-as-read-from-the-exe). Synthetic points only.

#include <cmath>
#include <cstdint>

#include <array>
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

namespace
{
uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The fence of the original's runs below, pulsed twice and faded for 33 ms, as the oracle ran it
LightSheet PulsedForceField()
{
	LightSheet sheet;
	sheet.StartForceField(LightSheet::k_ForceFieldRgb, LightSheet::k_ForceFieldHeight, LightSheet::k_ForceFieldPoints);
	const std::array<glm::vec3, 3> fence {glm::vec3 {10.0f, 5.0f, 20.0f}, {30.5f, 6.25f, 22.0f}, {40.0f, 4.0f, -3.5f}};
	sheet.SetForceFieldPoints(fence);
	sheet.PulseForceField({12.0f, 0.0f, 21.0f}, 25.0f);
	sheet.PulseForceField({38.0f, 99.0f, -1.0f}, 60.0f);
	return sheet;
}
} // namespace

TEST(LightSheet, AForceFieldStandsOnItsFenceClosedAndAPulseLightsItAcrossTheGround)
{
	auto sheet = PulsedForceField();
	ASSERT_EQ(sheet.Strengths().size(), LightSheet::k_ForceFieldPoints);
	// the fence's three corners and the first again; the pulses' strengths as the original's (oracle)
	EXPECT_EQ(Bits(sheet.Strengths()[0]), 0x3FA9E27Au);
	EXPECT_EQ(Bits(sheet.Strengths()[1]), 0x3F5B1082u);
	EXPECT_EQ(Bits(sheet.Strengths()[2]), 0x3F72570Au);
	EXPECT_EQ(Bits(sheet.Strengths()[3]), 0x3FA9E27Au);
	EXPECT_EQ(sheet.Strengths()[4], 0.0f);
}

TEST(LightSheet, AForceFieldsLightFadesAsTheGameWorksOutTheExponential)
{
	auto sheet = PulsedForceField();
	sheet.Update(0.033f);
	EXPECT_EQ(Bits(sheet.Strengths()[0]), 0x3FA45EB6u);
	EXPECT_EQ(Bits(sheet.Strengths()[1]), 0x3F53F40Du);
	EXPECT_EQ(Bits(sheet.Strengths()[2]), 0x3F6A7929u);
	// a strength of 1, 0.75 and 0.1 faded for 50 ms and 16 ms, as the original's (oracle)
	const auto faded = [](float strength, float seconds) {
		return Bits(strength + ((0.0f - strength) * LightSheet::ForceFieldFade(seconds)));
	};
	EXPECT_EQ(faded(1.0f, 0.05f), 0x3F7383C5u);
	EXPECT_EQ(faded(0.75f, 0.016f), 0x3F3CF3D4u);
	EXPECT_EQ(faded(0.1f, 0.016f), 0x3DC98C9Eu);
	EXPECT_EQ(faded(1.0f, 0.033f), 0x3F77B09Bu);
}

TEST(LightSheet, AForceFieldIsBuiltTwiceOverItsTextureSlidingBothWays)
{
	auto sheet = PulsedForceField();
	sheet.Update(0.033f);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 2u * 4u * 3u);
	ASSERT_EQ(triangles.size(), 2u * 3u * 12u);
	// the middle of the four points, as the original's (oracle)
	EXPECT_EQ(Bits(sheet.Middle().x), 0x41B50000u);
	EXPECT_EQ(Bits(sheet.Middle().y), 0x40A20000u);
	EXPECT_EQ(Bits(sheet.Middle().z), 0x416A0000u);
	// the second pass's corners, colours and texture places, as the original's last draw left them (oracle)
	constexpr std::array<uint32_t, 36> k_Positions {
	    0x41200000u, 0x40A00000u, 0x41A00000u, 0x41200000u, 0x43020000u, 0x41A00000u, 0x41200000u, 0x437F0000u, 0x41A00000u,
	    0x41F40000u, 0x40C80000u, 0x41B00000u, 0x41F40000u, 0x43034000u, 0x41B00000u, 0x41F40000u, 0x43802000u, 0x41B00000u,
	    0x42200000u, 0x40800000u, 0xC0600000u, 0x42200000u, 0x43010000u, 0xC0600000u, 0x42200000u, 0x437E0000u, 0xC0600000u,
	    0x41200000u, 0x40A00000u, 0x41A00000u, 0x41200000u, 0x43020000u, 0x41A00000u, 0x41200000u, 0x437F0000u, 0x41A00000u,
	};
	constexpr std::array<uint32_t, 24> k_Colours {
	    0xFF808080u, 0x00000000u, 0xFF808080u, 0x20404040u, 0x00808080u, 0x00000000u, 0xD3808080u, 0x00000000u,
	    0xD3808080u, 0x20404040u, 0x00808080u, 0x00000000u, 0xE9808080u, 0x00000000u, 0xE9808080u, 0x20404040u,
	    0x00808080u, 0x00000000u, 0xFF808080u, 0x00000000u, 0xFF808080u, 0x20404040u, 0x00808080u, 0x00000000u,
	};
	constexpr std::array<uint32_t, 24> k_Uvs {
	    0x3BD844D0u, 0x00000000u, 0x3BD844D0u, 0x3F000000u, 0x3BD844D0u, 0x3F800000u, 0x3E5A1028u, 0x00000000u,
	    0x3E5A1028u, 0x3F000000u, 0x3E5A1028u, 0x3F800000u, 0x3EF8D546u, 0x00000000u, 0x3EF8D546u, 0x3F000000u,
	    0x3EF8D546u, 0x3F800000u, 0x3F5E01F5u, 0x00000000u, 0x3F5E01F5u, 0x3F000000u, 0x3F5E01F5u, 0x3F800000u,
	};
	for (size_t i = 0; i < 12; ++i)
	{
		const auto& second = vertices[12 + i];
		EXPECT_EQ(Bits(second.position.x), k_Positions.at(i * 3)) << i;
		EXPECT_EQ(Bits(second.position.y), k_Positions.at((i * 3) + 1)) << i;
		EXPECT_EQ(Bits(second.position.z), k_Positions.at((i * 3) + 2)) << i;
		EXPECT_EQ(second.argb, k_Colours.at(i * 2)) << i;
		EXPECT_EQ(second.specularArgb, k_Colours.at((i * 2) + 1)) << i;
		EXPECT_EQ(Bits(second.uv.x), k_Uvs.at(i * 2)) << i;
		EXPECT_EQ(Bits(second.uv.y), k_Uvs.at((i * 2) + 1)) << i;
		// the first pass is the same but for its texture sliding the other way
		const auto& first = vertices[i];
		EXPECT_EQ(first.position, second.position) << i;
		EXPECT_EQ(first.argb, second.argb) << i;
		EXPECT_EQ(first.uv.y, second.uv.y) << i;
	}
	// the slide kept a fifth of itself, -0.0066 (oracle): the first pass's texture runs on from it
	EXPECT_EQ(Bits(vertices[0].uv.x), 0xBBD844D0u);
	EXPECT_EQ(triangles[36], 12u);
}

TEST(LightSheet, AForceFieldWithNoPointLitAboveAFiftiethIsNotBuilt)
{
	LightSheet sheet;
	sheet.StartForceField(LightSheet::k_ForceFieldRgb, LightSheet::k_ForceFieldHeight, LightSheet::k_ForceFieldPoints);
	const std::array<glm::vec3, 2> fence {glm::vec3 {0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}};
	sheet.SetForceFieldPoints(fence);
	// a pulse of no radius lights nothing; one halfway along lights each end 1 - 0.5 / 0.51, under a fiftieth
	sheet.PulseForceField({0.0f, 0.0f, 0.0f}, 0.0f);
	sheet.PulseForceField({0.5f, 0.0f, 0.0f}, 0.51f);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	EXPECT_TRUE(vertices.empty());
	EXPECT_TRUE(triangles.empty());
}

TEST(LightSheet, AGesturesWaveRollsAtTheGamesSinglePrecision)
{
	// seven points 7.5 high, 33 ms after 1.234 s, as the original's (oracle)
	LightSheet sheet;
	sheet.Start(Row(7, 1.0f), 0xFFFFFFu, 7.5f, 0.03f);
	sheet.Update(1.234f);
	sheet.Update(0.033f);
	constexpr std::array<uint32_t, 7> k_Heights {
	    0x4087182Bu, 0x40F8E7D7u, 0x4087182Bu, 0x40F8E7D7u, 0x4087182Au, 0x40F8E7D5u, 0x4087182Au,
	};
	for (size_t i = 0; i < k_Heights.size(); ++i)
	{
		EXPECT_EQ(Bits(sheet.Heights()[i]), k_Heights.at(i)) << i;
	}
}

TEST(LightSheet, AGesturesSheetIsBuiltAtTheGamesSinglePrecision)
{
	// three points, fed 0.7 for 40 ms and 0.9 for 50 more, as the original's run (oracle)
	LightSheet sheet;
	sheet.Start({{10.3f, 5.1f, 20.7f}, {30.5f, 6.25f, 22.1f}, {40.9f, 4.3f, -3.5f}}, 0x80FF40u, 9.0f, 0.03f);
	sheet.SetStrength(0.7f);
	sheet.Update(0.04f);
	sheet.SetStrength(0.9f);
	sheet.Update(0.05f);
	EXPECT_EQ(Bits(sheet.Strengths()[2]), 0x3F333333u);
	EXPECT_EQ(Bits(sheet.Heights()[1]), 0x40932156u);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 9u);
	// the middle: the float of one third times the sum
	EXPECT_EQ(Bits(sheet.Middle().x), 0x41D9DDDEu);
	EXPECT_EQ(Bits(sheet.Middle().y), 0x40A6EEF0u);
	EXPECT_EQ(Bits(sheet.Middle().z), 0x4151999Bu);
	constexpr std::array<uint32_t, 27> k_Positions {
	    0x4124CCCDu, 0x40A33333u, 0x41A5999Au, 0x411E06D3u, 0x40E9AEB0u, 0x41A71EB9u, 0x4109B4E6u, 0x415E9093u, 0x41ABAE15u,
	    0x41F40000u, 0x40C80000u, 0x41B0CCCDu, 0x41F4A741u, 0x40E9EE54u, 0x41B2999Au, 0x41F69D04u, 0x4127DCA8u, 0x41B80000u,
	    0x4223999Au, 0x4089999Au, 0xC0600000u, 0x4224F778u, 0x40BFC246u, 0xC07A8F5Du, 0x42291112u, 0x41311E26u, 0xC0A51EBAu,
	};
	constexpr std::array<uint32_t, 18> k_Colours {
	    0xFF000000u, 0x00000000u, 0xFF7FFE3Fu, 0x203F7F1Fu, 0xFF000000u, 0x00000000u, 0xFF000000u, 0x00000000u, 0xFF67CD33u,
	    0x20336619u, 0xFF000000u, 0x00000000u, 0xFF000000u, 0x00000000u, 0xFF7FFE3Fu, 0x203F7F1Fu, 0xFF000000u, 0x00000000u,
	};
	constexpr std::array<uint32_t, 18> k_Uvs {
	    0x00000000u, 0x3F8B851Fu, 0x00000000u, 0x3F570A3Eu, 0x00000000u, 0x3DB851ECu, 0x4001CC89u, 0x3F8B851Fu, 0x4001CC89u,
	    0x3F570A3Eu, 0x4001CC89u, 0x3DB851ECu, 0x40998A97u, 0x3F8B851Fu, 0x40998A97u, 0x3F570A3Eu, 0x40998A97u, 0x3DB851ECu,
	};
	for (size_t i = 0; i < vertices.size(); ++i)
	{
		EXPECT_EQ(Bits(vertices[i].position.x), k_Positions.at(i * 3)) << i;
		EXPECT_EQ(Bits(vertices[i].position.y), k_Positions.at((i * 3) + 1)) << i;
		EXPECT_EQ(Bits(vertices[i].position.z), k_Positions.at((i * 3) + 2)) << i;
		EXPECT_EQ(vertices[i].argb, k_Colours.at(i * 2)) << i;
		EXPECT_EQ(vertices[i].specularArgb, k_Colours.at((i * 2) + 1)) << i;
		EXPECT_EQ(Bits(vertices[i].uv.x), k_Uvs.at(i * 2)) << i;
		EXPECT_EQ(Bits(vertices[i].uv.y), k_Uvs.at((i * 2) + 1)) << i;
	}
}
