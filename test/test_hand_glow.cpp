/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's glow as a draw for the renderer (src/Graphics/HandGlow.h), against the wiki (docs/bw1-notes/magic.md, "The
// hand"): a second pass in the player's colour at 0.8 while a miracle is in the hand, none without one, the flowing
// texture's cell of an 8 x 4 sheet stepped at -20 frames a second. Synthetic values only.

#include <cstdint>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/FrameAnim.h"
#include "Graphics/HandGlow.h"
#include "Magic/Hand/HandMagicFX.h"

using namespace openblack;

namespace
{
/// Player one's colour in the player colour table (0xAARRGGBB)
constexpr uint32_t k_PlayerOne = 0xFFFF4646;
} // namespace

TEST(HandGlow, noneWithoutAMiracleInTheHand)
{
	EXPECT_FALSE(graphics::HandGlowOf(magic::hand_fx::Glow {}, k_PlayerOne).has_value());
	EXPECT_FALSE(
	    graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.0f, .uvOffset = {0.5f, 0.25f}}, k_PlayerOne).has_value());
}

TEST(HandGlow, fourFifthsInThePlayersColour)
{
	const auto draw = graphics::HandGlowOf(magic::hand_fx::Glow {.alpha = 0.8f, .uvOffset = {0.25f, 0.125f}}, k_PlayerOne);
	ASSERT_TRUE(draw.has_value());
	EXPECT_FLOAT_EQ(draw->alpha, 0.8f);
	EXPECT_FLOAT_EQ(draw->uvOffset.x, 0.25f);
	EXPECT_FLOAT_EQ(draw->uvOffset.y, 0.125f);
	EXPECT_FLOAT_EQ(draw->colour.r, 1.0f);
	EXPECT_FLOAT_EQ(draw->colour.g, 70.0f / 255.0f);
	EXPECT_FLOAT_EQ(draw->colour.b, 70.0f / 255.0f);
}

TEST(HandGlow, theColoursAlphaIsNotRead)
{
	const magic::hand_fx::Glow glow {.alpha = 0.8f};
	const auto opaque = graphics::HandGlowOf(glow, 0xFF47FF54);
	const auto clear = graphics::HandGlowOf(glow, 0x0047FF54);
	ASSERT_TRUE(opaque.has_value());
	ASSERT_TRUE(clear.has_value());
	EXPECT_EQ(opaque->colour, clear->colour);
	EXPECT_FLOAT_EQ(opaque->alpha, clear->alpha);
}

TEST(HandGlow, theFlowingCellOfTheFrame)
{
	// -20 frames a second from 0: 0.03 s later the frame is -0.6, wrapped to 63.4 and rounded to 63, cell 63 % 32 = 31,
	// the last of the 8 x 4 sheet
	float phase = 0.0f;
	const magic::hand_fx::Glow glow {.alpha = 0.8f, .uvOffset = graphics::frame_anim::HandFlowFrame(phase, 0.03f)};
	const auto draw = graphics::HandGlowOf(glow, k_PlayerOne);
	ASSERT_TRUE(draw.has_value());
	EXPECT_FLOAT_EQ(draw->uvOffset.x, 7.0f * 0.125f);
	EXPECT_FLOAT_EQ(draw->uvOffset.y, 3.0f * 0.125f);
	// a whole turn of the sheet (32 frames, 1.6 s) later the cell is the same
	const magic::hand_fx::Glow later {.alpha = 0.8f, .uvOffset = graphics::frame_anim::HandFlowFrame(phase, 1.6f)};
	const auto again = graphics::HandGlowOf(later, k_PlayerOne);
	ASSERT_TRUE(again.has_value());
	EXPECT_FLOAT_EQ(again->uvOffset.x, draw->uvOffset.x);
	EXPECT_FLOAT_EQ(again->uvOffset.y, draw->uvOffset.y);
}
