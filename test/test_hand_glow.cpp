/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The glow over a hand holding a miracle as a draw for the renderer (src/Graphics/HandGlow.h), against the wiki
// (docs/wiki/magic/hand-casting.md#the-hand): drawn only while its strength is above a hundredth, that strength the
// alpha of the hand's colour in whole 255ths (204 for 0.8), the player's colour in place of the land's light, and the
// flow's cell of an 8 x 4 sheet stepped at -20 frames a second. Synthetic values only.

#include <cstdint>

#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "Graphics/HandGlow.h"
#include "Magic/Hand/HandMagicFX.h"

using namespace openblack;

namespace
{
/// Player one's colour in the player colour table (0xAARRGGBB)
constexpr uint32_t k_PlayerOne = 0xFFFF4646;
/// The glow's strength while a miracle is in the hand
constexpr float k_Held = 0.8f;
} // namespace

TEST(HandGlow, noneWithoutAMiracleInTheHand)
{
	EXPECT_FALSE(graphics::HandGlowOf(magic::hand_fx::Glow {}, k_PlayerOne).has_value());
	EXPECT_FALSE(
	    graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.0f, .uvOffset = {0.5f, 0.25f}}, k_PlayerOne).has_value());
}

TEST(HandGlow, notDrawnAtAHundredthOrLess)
{
	EXPECT_FALSE(graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.01f}, k_PlayerOne).has_value());
	const auto faint = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.0101f}, k_PlayerOne);
	ASSERT_TRUE(faint.has_value());
	// 2.58 of 255, the fraction dropped
	EXPECT_EQ(faint->alpha, 2);
}

TEST(HandGlow, fourFifthsInThePlayersColour)
{
	const auto draw = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = k_Held, .uvOffset = {0.25f, 0.125f}}, k_PlayerOne);
	ASSERT_TRUE(draw.has_value());
	// 0.8 x 255 with the fraction dropped
	EXPECT_EQ(draw->alpha, 204);
	EXPECT_EQ(draw->rgb, 0x00FF4646u);
	// column 2, row 1
	EXPECT_EQ(draw->cell, 10);
}

TEST(HandGlow, theStrengthsFractionIsDropped)
{
	// 127.5 of 255
	const auto half = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.5f}, k_PlayerOne);
	ASSERT_TRUE(half.has_value());
	EXPECT_EQ(half->alpha, 127);
	const auto full = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 1.0f}, k_PlayerOne);
	ASSERT_TRUE(full.has_value());
	EXPECT_EQ(full->alpha, 255);
}

TEST(HandGlow, theColoursAlphaIsNotRead)
{
	const magic::hand_fx::Glow glow {.alpha = k_Held};
	const auto opaque = graphics::HandGlowOf(glow, 0xFF47FF54);
	const auto clear = graphics::HandGlowOf(glow, 0x0047FF54);
	ASSERT_TRUE(opaque.has_value());
	ASSERT_TRUE(clear.has_value());
	EXPECT_EQ(opaque->rgb, clear->rgb);
	EXPECT_EQ(opaque->rgb, 0x0047FF54u);
	EXPECT_EQ(opaque->alpha, clear->alpha);
}

TEST(HandGlow, theFlowingCellOfTheFrame)
{
	// -20 frames a second from 0: 0.03 s later the frame is -0.6, wrapped to 63.4 and rounded to 63, cell 63 % 32 = 31,
	// the last of the 8 x 4 sheet
	float phase = 0.0f;
	const magic::hand_fx::Glow glow {.alpha = k_Held, .uvOffset = graphics::frame_anim::HandFlowFrame(phase, 0.03f)};
	const auto draw = graphics::HandGlowOf(glow, k_PlayerOne);
	ASSERT_TRUE(draw.has_value());
	EXPECT_EQ(draw->cell, 31);
	// a whole turn of the sheet (32 frames, 1.6 s) later the cell is the same
	const magic::hand_fx::Glow later {.alpha = k_Held, .uvOffset = graphics::frame_anim::HandFlowFrame(phase, 1.6f)};
	const auto again = graphics::HandGlowOf(later, k_PlayerOne);
	ASSERT_TRUE(again.has_value());
	EXPECT_EQ(again->cell, draw->cell);
}

TEST(HandGlow, everyCellOfTheSheetFromItsOffset)
{
	for (int cell = 0; cell < 32; ++cell)
	{
		const int column = cell % 8;
		const int row = cell / 8;
		const glm::vec2 offset {static_cast<float>(column) * 0.125f, static_cast<float>(row) * 0.125f};
		EXPECT_EQ(graphics::HandGlowCell(offset), cell);
	}
}

TEST(HandGlow, theRowIsTheHandsInThePlayersColourAtTheGlowsAlpha)
{
	glm::mat4 hand(1.0f);
	hand[1].w = 2.5f;                                    // the texture offset
	hand[3] = {10.0f, 20.0f, 30.0f, 1.0f};               // where the hand is
	hand[0].w = 0.0f;                                    // not faded
	const glm::vec4 colours {0.0f, 1234.0f, 0.0f, 0.0f}; // the land's light, a specular, no window, no flags
	const auto draw = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = k_Held}, k_PlayerOne);
	ASSERT_TRUE(draw.has_value());
	const auto row = graphics::HandGlowRow(hand, colours, *draw);
	// the hand's matrix and texture offset as they are
	EXPECT_EQ(glm::vec3(row[0]), glm::vec3(hand[0]));
	EXPECT_EQ(row[1], hand[1]);
	EXPECT_EQ(row[2], hand[2]);
	EXPECT_EQ(row[3], hand[3]);
	// faded to 204 of 255
	EXPECT_FLOAT_EQ(row[0].w, 1.0f - (204.0f / 255.0f));
	// the player's colour set in place of the land's light, the specular and flags kept
	EXPECT_FLOAT_EQ(row[4].x, 1.0f + static_cast<float>(0x00FF4646u));
	EXPECT_FLOAT_EQ(row[4].y, 1234.0f);
	EXPECT_FLOAT_EQ(row[4].z, 0.0f);
	EXPECT_FLOAT_EQ(row[4].w, 0.0f);
}

TEST(HandGlow, theCellRidesInTheWindowUniform)
{
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(std::nullopt), 0.0f);
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(uint8_t {0}), 1.0f);
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(uint8_t {31}), 32.0f);
}

// The hand's body lit by the posed models' rule rides there too, below zero; the glow's cell wins over it
TEST(HandGlow, thePosedLightRidesInTheWindowUniformBelowZero)
{
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(std::nullopt, true), -1.0f);
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(std::nullopt, false), 0.0f);
	EXPECT_FLOAT_EQ(graphics::HandGlowWindowZ(uint8_t {3}, true), 4.0f);
}

// The glow is on for a thing of the magic or the magic living kind in the hand, or a spell seed; nothing else
// (docs/wiki/magic/hand-casting.md#the-hand)
TEST(HandGlow, aMagicOrMagicLivingThingOrASpellSeedInTheHandMakesItGlow)
{
	using magic::hand_fx::GlowsInHand;
	EXPECT_TRUE(GlowsInHand(ObjectType::Magic, false));
	EXPECT_TRUE(GlowsInHand(ObjectType::MagicLiving, false));
	EXPECT_TRUE(GlowsInHand(std::nullopt, true));
	EXPECT_TRUE(GlowsInHand(ObjectType::Villager, true));
	EXPECT_FALSE(GlowsInHand(ObjectType::Villager, false));
	EXPECT_FALSE(GlowsInHand(ObjectType::Animal, false));
	EXPECT_FALSE(GlowsInHand(std::nullopt, false));
}
