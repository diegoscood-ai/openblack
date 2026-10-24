/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The 3D model inside a gesture icon: which model an icon shows, where on the screen it is moved to, the box whose
// depth is reset under it, the leash's spin, the leash and bubble matrices, the alpha byte and the bubble's far cut. The
// numbers are the original game's own, bit for bit, read by running its draw of the model

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <limits>

#include <gtest/gtest.h>

#include "Magic/Gestures/GestureIconModels.h"

using namespace openblack::magic::gestures;

namespace
{
GestureIcon IconOf(int order, int seed, int leash)
{
	GestureIcon icon;
	icon.order = order;
	icon.seed = seed;
	icon.leash = leash;
	return icon;
}

uint32_t Bits(float value)
{
	return std::bit_cast<uint32_t>(value);
}

/// The spin's rows, nine words, row by row
std::array<uint32_t, 9> SpinBits(const LeashSpin& spin)
{
	std::array<uint32_t, 9> out {};
	for (size_t r = 0; r < 3; ++r)
	{
		for (int j = 0; j < 3; ++j)
		{
			out.at((r * 3) + static_cast<size_t>(j)) = Bits(spin.rows.at(r)[j]);
		}
	}
	return out;
}

/// A matrix's three axes as rows (the game's layout), nine words, then its place
std::array<uint32_t, 12> MatrixBits(const glm::mat4& matrix)
{
	std::array<uint32_t, 12> out {};
	for (int c = 0; c < 4; ++c)
	{
		for (int j = 0; j < 3; ++j)
		{
			out.at(static_cast<size_t>((c * 3) + j)) = Bits(matrix[c][j]);
		}
	}
	return out;
}
} // namespace

TEST(GestureIconModels, TheLeashComesFirstThenTheSeedThenTheBubbleForTheSpiral)
{
	EXPECT_EQ(IconModelKindOf(IconOf(4, 3, 2)), IconModelKind::Leash);
	EXPECT_EQ(IconModelKindOf(IconOf(0xC, 3, -1)), IconModelKind::SeedGraphic);
	EXPECT_EQ(IconModelKindOf(IconOf(8, -1, -1)), IconModelKind::Bubble);
	EXPECT_EQ(IconModelKindOf(IconOf(8, 5, -1)), IconModelKind::SeedGraphic);
	EXPECT_EQ(IconModelKindOf(IconOf(0xA, -1, -1)), IconModelKind::None);
	// the inverse spiral always shows the creature's miracles' seed
	EXPECT_EQ(IconModelSeed(IconOf(9, -1, -1)), k_InverseSpiralSeed);
	EXPECT_EQ(IconModelKindOf(IconOf(9, -1, -1)), IconModelKind::SeedGraphic);
	EXPECT_EQ(IconModelSeed(IconOf(8, 7, -1)), 7);
}

TEST(GestureIconModels, TheModelIsMovedFromTheMiddleOfTheScreenToTheBoxAboveTheIcon)
{
	struct Case
	{
		IconBox box;
		glm::ivec2 screen;
		uint32_t x;
		uint32_t y;
	};
	const std::array<Case, 4> k_Cases {{
	    {{{972.8f, 732.16f}, {1008.64f, 768.0f}}, {1024, 768}, 0x43EF5C28u, 0x43A51EB8u},
	    {{{1797.12f, 1012.8f}, {1864.32f, 1080.0f}}, {1920, 1080}, 0x4459AE14u, 0x43DB9998u},
	    // an odd screen: its halves are whole pixels
	    {{{700.0f, 551.5f}, {728.0f, 579.0f}}, {801, 579}, 0x439D0000u, 0x4378C000u},
	    {{{10.25f, 20.75f}, {30.5f, 40.125f}}, {640, 481}, 0xC395D000u, 0xC364F000u},
	}};
	for (const auto& c : k_Cases)
	{
		const auto offset = IconModelScreenOffset(c.box, c.screen);
		EXPECT_EQ(Bits(offset.x), c.x);
		EXPECT_EQ(Bits(offset.y), c.y);
	}
}

TEST(GestureIconModels, TheDepthIsResetInTheBoxAboveTheIcon)
{
	const IconBox box {{972.8f, 732.16f}, {1008.64f, 768.0f}};
	const auto corners = IconModelDepthBox(box);
	EXPECT_EQ(corners[0], glm::vec2(972.8f, 732.16f));
	EXPECT_EQ(corners[1], glm::vec2(1008.64f, 732.16f));
	EXPECT_EQ(Bits(corners[2].y), 0x442E147Au); // 2 x 732.16 - 768
	EXPECT_EQ(corners[2].x, 1008.64f);
	EXPECT_EQ(corners[3], glm::vec2(972.8f, corners[2].y));
}

TEST(GestureIconModels, ALeashsSpinTurnsAsTheOriginalsDoes)
{
	LeashSpin spin;
	constexpr std::array<std::array<uint32_t, 9>, 4> k_Leash2At33Ms {{
	    {0x3F7FBFB0u, 0xBCCA47BEu, 0x3D169C2Du, 0x3CCFFE28u, 0x3F7FDF81u, 0xBC9888F7u, 0xBD14A6F3u, 0x3CA00883u, 0x3F7FC850u},
	    {0x3F7EFEE5u, 0xBD47304Au, 0x3D9769EEu, 0x3D529B6Au, 0x3F7F7E1Au, 0xBD149BC4u, 0xBD938010u, 0x3D23989Eu, 0x3F7F2161u},
	    {0x3F7DBE12u, 0xBD92F56Cu, 0x3DE42630u, 0x3D9FCABFu, 0x3F7EDC04u, 0xBD58DAC0u, 0xBDDB5A2Eu, 0x3D7A8B4Cu, 0x3F7E0B96u},
	    {0x3F7BFDF6u, 0xBDC08D90u, 0x3E18AA93u, 0x3DD75634u, 0x3F7DF9A0u, 0xBD8C6C65u, 0xBE10DB83u, 0x3DAA5440u, 0x3F7C8795u},
	}};
	for (const auto& expected : k_Leash2At33Ms)
	{
		StepLeashSpin(spin, 2, 33);
		EXPECT_EQ(SpinBits(spin), expected);
	}
	// the leash's model: those rows times 0.15, ten units along z
	constexpr std::array<uint32_t, 12> k_Model {0x3E1731FAu, 0xBC671047u, 0x3CB73317u, 0x3C8133B9u, 0x3E186294u, 0xBC288213u,
	                                            0xBCADD437u, 0x3C4C651Au, 0x3E17848Du, 0x00000000u, 0x00000000u, 0x41200000u};
	EXPECT_EQ(MatrixBits(LeashModelMatrix(spin)), k_Model);
	// a longer frame
	StepLeashSpin(spin, 2, 100);
	constexpr std::array<uint32_t, 9> k_After100Ms {0x3F73B677u, 0xBE1D246Eu, 0x3E879940u, 0x3E3F9C58u, 0x3F79FCDFu,
	                                                0xBDDAB6AAu, 0xBE780BDCu, 0x3E1ADAB5u, 0x3F755718u};
	EXPECT_EQ(SpinBits(spin), k_After100Ms);
	// the other leashes from unturned
	LeashSpin evil;
	StepLeashSpin(evil, 1, 33);
	constexpr std::array<uint32_t, 9> k_Leash1 {0x3F7FC2AEu, 0xBCCE50AAu, 0x3D100275u, 0x3CD41638u, 0x3F7FDD46u,
	                                            0xBCA1A091u, 0xBD0DE5E3u, 0x3CA8EEC2u, 0x3F7FCAB7u};
	EXPECT_EQ(SpinBits(evil), k_Leash1);
	LeashSpin good;
	StepLeashSpin(good, 3, 33);
	constexpr std::array<uint32_t, 9> k_Leash3 {0x3F7FBCC0u, 0xBCC4FC12u, 0x3D1D3416u, 0x3CCA9C19u, 0x3F7FE1E4u,
	                                            0xBC8F8163u, 0xBD1B67E8u, 0x3C972260u, 0x3F7FC5A6u};
	EXPECT_EQ(SpinBits(good), k_Leash3);
}

TEST(GestureIconModels, TheLeashShowsItsBandOfTheTexture)
{
	EXPECT_EQ(LeashModelBand(1), 0.375f);
	EXPECT_EQ(LeashModelBand(2), 0.125f);
	EXPECT_EQ(LeashModelBand(3), 0.25f);
}

TEST(GestureIconModels, TheBubbleIsATenthTurnedAQuarterAboutX)
{
	constexpr std::array<uint32_t, 12> k_Bubble {0x3DCCCCCDu, 0x00000000u, 0x00000000u, 0x80000000u, 0xB19630F2u, 0xBDCCCCCDu,
	                                             0x00000000u, 0x3DCCCCCDu, 0xB19630F2u, 0x00000000u, 0x00000000u, 0x41200000u};
	EXPECT_EQ(MatrixBits(BubbleModelMatrix()), k_Bubble);
}

TEST(GestureIconModels, TheBubbleIsLeftOutWhenItsReachIsShortOfItsDistance)
{
	// 30 x its half diagonal, squared, against the squared distance 104 from the camera
	EXPECT_TRUE(BubbleModelDrawn(0.34f));
	EXPECT_TRUE(BubbleModelDrawn(0.33994f));
	EXPECT_FALSE(BubbleModelDrawn(0.33993f));
	EXPECT_FALSE(BubbleModelDrawn(0.0f));
	EXPECT_FALSE(BubbleModelDrawn(std::numeric_limits<float>::quiet_NaN()));
}

TEST(GestureIconModels, TheColourTakesTheIconsAlphaByteTruncated)
{
	EXPECT_EQ(IconModelAlpha(1.0f), 0xFF);
	EXPECT_EQ(IconModelAlpha(0.5f), 0x7F);
	EXPECT_EQ(IconModelAlpha(0.123f), 0x1F);
	EXPECT_EQ(IconModelAlpha(0.999f), 0xFE);
	EXPECT_EQ(IconModelAlpha(0.0039f), 0x00);
}

TEST(GestureIconModels, ALeashsSpinIsMadeAtItsFirstDrawAndTurnsWithEveryDraw)
{
	LeashSpins spins;
	const auto rope = IconOf(5, -1, 2);
	auto draw = StepIconModel(rope, spins, 0.5f, 33, 0.0f);
	ASSERT_EQ(draw.kind, IconModelKind::Leash);
	ASSERT_TRUE(spins.at(2).has_value());
	EXPECT_FALSE(spins.at(1).has_value());
	EXPECT_EQ(draw.alpha, 0x7F);
	EXPECT_EQ(draw.band, 0.125f);
	LeashSpin once;
	StepLeashSpin(once, 2, 33);
	EXPECT_EQ(SpinBits(*spins.at(2)), SpinBits(once));
	EXPECT_EQ(MatrixBits(draw.matrix), MatrixBits(LeashModelMatrix(once)));
	// two icons of the same leash in a frame turn it twice
	(void)StepIconModel(rope, spins, 1.0f, 33, 0.0f);
	StepLeashSpin(once, 2, 33);
	EXPECT_EQ(SpinBits(*spins.at(2)), SpinBits(once));
	// (openblack guard) a leash number past the game's table shows nothing
	EXPECT_EQ(StepIconModel(IconOf(5, -1, 4), spins, 1.0f, 33, 0.0f).kind, IconModelKind::None);
}

TEST(GestureIconModels, TheBubbleAndTheSeedDrawsOfAFrame)
{
	LeashSpins spins;
	const auto spiral = IconOf(8, -1, -1);
	const auto shown = StepIconModel(spiral, spins, 1.0f, 33, 2.0f);
	EXPECT_EQ(shown.kind, IconModelKind::Bubble);
	EXPECT_EQ(MatrixBits(shown.matrix), MatrixBits(BubbleModelMatrix()));
	EXPECT_EQ(StepIconModel(spiral, spins, 1.0f, 33, 0.0f).kind, IconModelKind::None);
	// a seed's graphic is drawn by its own module (GestureIconSeeds.h): its kind says so, and no spin is made
	EXPECT_EQ(StepIconModel(IconOf(0xE, 4, -1), spins, 1.0f, 33, 2.0f).kind, IconModelKind::SeedGraphic);
	EXPECT_FALSE(std::ranges::any_of(spins, [](const auto& spin) { return spin.has_value(); }));
}
