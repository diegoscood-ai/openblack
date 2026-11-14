/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a worship icon draws while it charges (src/Worship/IconChargeVisual.h), pinned to the original game's own charge
// draw run on the same inputs (docs/wiki/magic/worship.md#the-charge-visual).

#include <cstdint>

#include <array>
#include <bit>
#include <vector>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Particles/LightSheet.h"
#include "Worship/IconChargeVisual.h"

using namespace openblack::worship;
using openblack::particles::LightSheet;

namespace
{
/// The first player's colour, red
constexpr uint32_t k_Red = 0xFFFF4646u;
constexpr uint32_t k_Green = 0xFF47FF54u;
constexpr uint32_t k_Black = 0xFF000000u;
constexpr uint32_t k_MsPerTurn = 100;

struct PhaseCase
{
	float phase;
	uint32_t tint;
};

/// A full icon (a seed held) at a phase of the particle system's clock, in red
constexpr std::array<PhaseCase, 14> k_FullCases {{
    {0.0f, 0xFF581818u},
    {0.03f, 0xFF701E1Eu},
    {0.0625f, 0xFF852424u},
    {0.1f, 0xFF952929u},
    {0.125f, 0xFF982929u},
    {0.2f, 0xFF7E2222u},
    {0.25f, 0xFF581818u},
    {0.3f, 0xFF330E0Eu},
    {0.375f, 0xFF190707u},
    {0.5f, 0xFF581818u},
    {0.6f, 0xFF952929u},
    {0.75f, 0xFF581818u},
    {0.875f, 0xFF190707u},
    {0.99f, 0xFF501616u},
}};

struct ChargingCase
{
	int32_t turns;
	float turnFraction;
	uint32_t tint;
};

/// A charging icon, its whole turns and the turn's fraction since the charge started, in red
constexpr std::array<ChargingCase, 18> k_ChargingCases {{
    {0, 0.0f, 0xFF310D0Du},
    {0, 0.5f, 0xFF390F0Fu},
    {1, 0.0f, 0xFF4E1515u},
    {1, 0.3f, 0xFF5F1A1Au},
    {2, 0.0f, 0xFF8C2626u},
    {2, 0.7f, 0xFFB43131u},
    {3, 0.0f, 0xFFBF3434u},
    {4, 0.0f, 0xFFC73636u},
    {5, 0.5f, 0xFF862424u},
    {7, 0.0f, 0xFF3D1010u},
    {8, 0.0f, 0xFF310D0Du},
    {9, 0.0f, 0xFF411212u},
    {10, 0.0f, 0xFF581818u},
    {12, 0.0f, 0xFF5A1818u},
    {14, 0.0f, 0xFF360F0Fu},
    {14, 0.9f, 0xFF310D0Du},
    {15, 0.0f, 0xFF310D0Du},
    {20, 0.0f, 0xFF310D0Du},
}};

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

using Bits3 = std::array<uint32_t, 3>;

/// The icon's matrix of the sheet's points, rows as the original keeps them: turned, sized 1 and 2, and placed
glm::mat4 Placed(float size, const glm::vec3& position)
{
	glm::mat4 model(0.0f);
	model[0] = glm::vec4(0.8f * size, 0.0f, -0.6f * size, 0.0f);
	model[1] = glm::vec4(0.0f, size, 0.0f, 0.0f);
	model[2] = glm::vec4(0.6f * size, 0.0f, 0.8f * size, 0.0f);
	model[3] = glm::vec4(position, 1.0f);
	return model;
}

/// The original's points for the two placements below
constexpr std::array<Bits3, 10> k_PointsOfSize1 {{
    {0x447A5333u, 0x41A20000u, 0x44FA04CDu},
    {0x447A5FE8u, 0x41A20000u, 0x44FA19BFu},
    {0x447A4EB5u, 0x41A20000u, 0x44FA2DE1u},
    {0x447A27A8u, 0x41A20000u, 0x44FA37C5u},
    {0x4479FD05u, 0x41A20000u, 0x44FA32CCu},
    {0x4479E2C1u, 0x41A20000u, 0x44FA2149u},
    {0x4479E525u, 0x41A20000u, 0x44FA0B6Eu},
    {0x447A0313u, 0x41A20000u, 0x44F9FB74u},
    {0x447A2E8Au, 0x41A20000u, 0x44F9F8D6u},
    {0x447A5333u, 0x41A20000u, 0x44FA04CDu},
}};
constexpr std::array<Bits3, 10> k_PointsOfSize2 {{
    {0x449A7333u, 0x425E0000u, 0x44768333u},
    {0x449A7FE8u, 0x425E0000u, 0x4476D6FDu},
    {0x449A6EB5u, 0x425E0000u, 0x44772782u},
    {0x449A47A8u, 0x425E0000u, 0x44774F15u},
    {0x449A1D05u, 0x425E0000u, 0x44773B31u},
    {0x449A02C1u, 0x425E0000u, 0x4476F525u},
    {0x449A0525u, 0x425E0000u, 0x44769DB8u},
    {0x449A2313u, 0x425E0000u, 0x44765DD2u},
    {0x449A4E8Au, 0x425E0000u, 0x44765359u},
    {0x449A7333u, 0x425E0000u, 0x44768333u},
}};
/// At the identity: the circle itself, the last point's sine of a whole turn not quite 0
constexpr std::array<Bits3, 10> k_PointsAtIdentity {{
    {0x3F800000u, 0x00000000u, 0x00000000u},
    {0x3F441B7Du, 0x00000000u, 0x3F248DBBu},
    {0x3E31D0CFu, 0x00000000u, 0x3F7C1C5Du},
    {0xBF000001u, 0x00000000u, 0x3F5DB3D7u},
    {0xBF708FB3u, 0x00000000u, 0x3EAF1D3Fu},
    {0xBF708FB1u, 0x00000000u, 0xBEAF1D4Cu},
    {0xBEFFFFFDu, 0x00000000u, 0xBF5DB3D8u},
    {0x3E31D0F2u, 0x00000000u, 0xBF7C1C5Bu},
    {0x3F441B80u, 0x00000000u, 0xBF248DB7u},
    {0x3F800000u, 0x00000000u, 0x343BBD2Eu},
}};

struct RingCase
{
	float phase;
	uint32_t alpha;
	std::array<uint32_t, 12> cells; ///< its three axes and its position, as the original's rows
};

/// The ring over an icon placed as Placed(1, (1000.5, 20.25, 2000.75)), whose mesh is 2.5 high from its middle
constexpr std::array<RingCase, 8> k_RingCases {{
    {0.10001f,
     0x00u,
     {0x4019984Bu, 0x00000000u, 0xBFE66470u, 0x00000000u, 0x403FFE5Du, 0x00000000u, 0x3FE66470u, 0x00000000u, 0x4019984Bu,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.15f,
     0x2Au,
     {0x4000068Eu, 0x00000000u, 0xBFC009D5u, 0x00000000u, 0x40200831u, 0x00000000u, 0x3FC009D5u, 0x00000000u, 0x4000068Eu,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.2f,
     0x54u,
     {0x3FCCE703u, 0x00000000u, 0xBF99AD43u, 0x00000000u, 0x40001062u, 0x00000000u, 0x3F99AD43u, 0x00000000u, 0x3FCCE703u,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.25f,
     0x7Fu,
     {0x3F99C0EBu, 0x00000000u, 0xBF66A161u, 0x00000000u, 0x3FC03126u, 0x00000000u, 0x3F66A161u, 0x00000000u, 0x3F99C0EBu,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.3f,
     0xA9u,
     {0x3F4D35A7u, 0x00000000u, 0xBF19E83Du, 0x00000000u, 0x3F804188u, 0x00000000u, 0x3F19E83Du, 0x00000000u, 0x3F4D35A7u,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.35f,
     0xD4u,
     {0x3ECDD2F3u, 0x00000000u, 0xBE9A5E37u, 0x00000000u, 0x3F00A3D8u, 0x00000000u, 0x3E9A5E37u, 0x00000000u, 0x3ECDD2F3u,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.4f,
     0xFEu,
     {0x3B1D48CDu, 0x00000000u, 0xBAEBED34u, 0x00000000u, 0x3B449B00u, 0x00000000u, 0x3AEBED34u, 0x00000000u, 0x3B1D48CDu,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
    {0.4003f,
     0xFEu,
     {0x36233333u, 0x00000000u, 0xB5F4CCCDu, 0x00000000u, 0x364C0000u, 0x00000000u, 0x35F4CCCDu, 0x00000000u, 0x36233333u,
      0x447A2000u, 0x41A60000u, 0x44FA1800u}},
}};

void ExpectPoints(const glm::mat4& model, const std::array<Bits3, 10>& expected)
{
	const auto points = charge_visual::SheetPoints(model);
	for (size_t i = 0; i < points.size(); ++i)
	{
		for (int k = 0; k < 3; ++k)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(points.at(i)[k]), expected.at(i).at(static_cast<size_t>(k)))
			    << "point " << i << " axis " << k;
		}
	}
}
} // namespace

TEST(IconChargeVisual, AFullIconPulsesInItsPlayersColourWithTheParticleClock)
{
	for (const auto& c : k_FullCases)
	{
		EXPECT_EQ(charge_visual::Tint(k_Red, true, true, c.phase, 0.0f), c.tint) << "phase " << c.phase;
	}
	// another player's colour, and the neutral player's black, whose tint is still not 0
	EXPECT_EQ(charge_visual::Tint(k_Green, true, true, 0.0625f, 0.0f), 0xFF25852Bu);
	EXPECT_EQ(charge_visual::Tint(k_Black, true, true, 0.0625f, 0.0f), 0xFF000000u);
}

TEST(IconChargeVisual, AChargingIconFlashesTwiceInItsFirstSecondAndAHalfThenStaysDim)
{
	for (const auto& c : k_ChargingCases)
	{
		const float seconds = charge_visual::ChargeSeconds(c.turns, c.turnFraction, k_MsPerTurn);
		EXPECT_EQ(charge_visual::Tint(k_Red, false, true, 0.05f, seconds), c.tint)
		    << c.turns << " turns and " << c.turnFraction;
	}
	EXPECT_EQ(charge_visual::Tint(k_Green, false, true, 0.05f, charge_visual::ChargeSeconds(10, 0.0f, k_MsPerTurn)),
	          0xFF18581Du);
	// the worship lesson's charge, held at three quarters after its flashes: the first player's red at 50 of 256
	EXPECT_EQ(charge_visual::Tint(k_Red, false, true, 0.05f, 2.0f), 0xFF310D0Du);
}

TEST(IconChargeVisual, AnIconNeitherFullNorChargingAddsNoColour)
{
	EXPECT_FALSE(charge_visual::Brightness(false, false, 0.25f, 0.5f).has_value());
	EXPECT_EQ(charge_visual::Tint(k_Red, false, false, 0.25f, 0.5f), 0u);
}

TEST(IconChargeVisual, TheChargeIsLiftedToASixthWhileCharging)
{
	// a seed held: full at 1, still 1 lifted
	auto level = charge_visual::Level(true, 0.0f, 0.3f, 5.0f, true);
	EXPECT_TRUE(level.full);
	EXPECT_EQ(Bits(level.fraction), 0x3F800000u);
	// a store and something still needed: the fraction lifted
	level = charge_visual::Level(false, 10.0f, 0.75f, 1.0f, true);
	EXPECT_FALSE(level.full);
	EXPECT_EQ(Bits(level.fraction), 0x3F4AAAAAu);
	// nothing more needed: full
	level = charge_visual::Level(false, 100.0f, 0.5f, 0.0f, true);
	EXPECT_TRUE(level.full);
	EXPECT_EQ(Bits(level.fraction), 0x3F155555u);
	// an empty store: the fraction is not read, so a charge just started shows a sixth
	level = charge_visual::Level(false, 0.0f, 0.75f, 1.0f, true);
	EXPECT_FALSE(level.full);
	EXPECT_EQ(Bits(level.fraction), 0x3E2AAAAAu);
	// not charging: the fraction as it is, or 0
	EXPECT_EQ(Bits(charge_visual::Level(false, 50.0f, 0.25f, 1.0f, false).fraction), 0x3E800000u);
	EXPECT_EQ(Bits(charge_visual::Level(false, 0.0f, 0.25f, 1.0f, false).fraction), 0u);
	EXPECT_EQ(Bits(charge_visual::Level(false, 1.0f, 0.3f, 1.0f, true).fraction), 0x3ED55555u);
}

TEST(IconChargeVisual, TheSheetStandsOnACircleThroughTheIconsMatrix)
{
	ExpectPoints(glm::mat4(1.0f), k_PointsAtIdentity);
	ExpectPoints(Placed(1.0f, {1000.5f, 20.25f, 2000.75f}), k_PointsOfSize1);
	ExpectPoints(Placed(2.0f, {1234.0f, 55.5f, 987.25f}), k_PointsOfSize2);
}

TEST(IconChargeVisual, TheSheetGivesNoLightWithNoChargeAndRisesWithIt)
{
	const auto circle = charge_visual::SheetPoints(glm::mat4(1.0f));
	LightSheet sheet;
	sheet.Start(std::vector<glm::vec3>(circle.begin(), circle.end()), k_Red, charge_visual::k_SheetHeight,
	            charge_visual::k_SheetShiftSeconds);
	sheet.SetSpread(charge_visual::k_SheetSpread);
	std::vector<LightSheet::Vertex> vertices;
	std::vector<uint32_t> triangles;
	// an icon not charging: fed 0 frame after frame, its corners stay black
	for (int frame = 0; frame < 30; ++frame)
	{
		sheet.SetStrength(0.0f);
		sheet.Update(0.016f);
	}
	sheet.Build(vertices, triangles);
	ASSERT_EQ(vertices.size(), 3 * charge_visual::k_SheetPoints);
	for (const auto& vertex : vertices)
	{
		EXPECT_EQ(vertex.argb & 0xFFFFFFu, 0u);
		EXPECT_EQ(vertex.specularArgb & 0xFFFFFFu, 0u);
	}
	// a charge just started (a sixth) runs along it a point every 0.03 s: after half a second every point has it
	for (int frame = 0; frame < 30; ++frame)
	{
		sheet.SetStrength(charge_visual::Level(false, 0.0f, 0.0f, 1.0f, true).fraction);
		sheet.Update(0.016f);
	}
	for (const float strength : sheet.Strengths())
	{
		EXPECT_EQ(Bits(strength), 0x3E2AAAAAu);
	}
	sheet.Build(vertices, triangles);
	// the bright line of each point is lit in red
	EXPECT_NE(vertices.at(1).argb & 0xFF0000u, 0u);
}

TEST(IconChargeVisual, TheRingShrinksFromThreeTimesTheIconRaisedAndFadingIn)
{
	const auto icon = Placed(1.0f, {1000.5f, 20.25f, 2000.75f});
	for (const auto& c : k_RingCases)
	{
		const auto pose = charge_visual::Ring(c.phase, 2.5f);
		ASSERT_TRUE(pose.has_value()) << "phase " << c.phase;
		EXPECT_EQ(pose->alpha, c.alpha) << "phase " << c.phase;
		const auto model = charge_visual::RingModel(icon, *pose);
		for (size_t i = 0; i < c.cells.size(); ++i)
		{
			EXPECT_EQ(std::bit_cast<uint32_t>(model[static_cast<int>(i / 3)][static_cast<int>(i % 3)]), c.cells.at(i))
			    << "phase " << c.phase << " cell " << i;
		}
	}
}

TEST(IconChargeVisual, TheRingIsOnlyDrawnInTheThirdOfThePhaseAfterItsStart)
{
	for (const float phase : {0.0f, 0.05f, 0.1f, 0.45f, 0.9f})
	{
		EXPECT_FALSE(charge_visual::Ring(phase, 2.5f).has_value()) << "phase " << phase;
	}
	EXPECT_TRUE(charge_visual::Ring(0.10001f, 2.5f).has_value());
	EXPECT_TRUE(charge_visual::Ring(0.4003f, 2.5f).has_value());
}
