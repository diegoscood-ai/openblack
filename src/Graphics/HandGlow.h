/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Graphics/ArgbColour.h"
#include "Magic/Hand/HandMagicFX.h"

/// The hand's glow as one draw for the renderer (Renderer::DrawHandGlow): while a miracle is in the hand, the hand is
/// drawn a second time over itself, added in the player's colour with the flowing texture, one cell of its 8 x 4 sheet
/// at a time. Nothing hands the renderer one today: the glow is computed (magic::hand_fx::GetGlow) but not drawn. Wiki:
/// docs/bw1-notes/magic.md, "The hand".
namespace openblack::graphics
{

/// One draw of the hand's glow
struct HandGlowDraw
{
	/// How much of it is added (0.8 while a miracle is in the hand)
	float alpha {0.0f};
	/// The flowing texture's cell of the frame, in eighths
	glm::vec2 uvOffset {0.0f, 0.0f};
	/// The player's colour, each channel of 1
	glm::vec3 colour {1.0f, 1.0f, 1.0f};
};

/// The draw of the hand's glow in a player's colour (0xAARRGGBB, its alpha not read), or none while nothing is in the
/// hand (alpha 0)
[[nodiscard]] inline std::optional<HandGlowDraw> HandGlowOf(const magic::hand_fx::Glow& glow, uint32_t playerRgb) noexcept
{
	if (glow.alpha <= 0.0f)
	{
		return std::nullopt;
	}
	return HandGlowDraw {.alpha = glow.alpha, .uvOffset = glow.uvOffset, .colour = argb_colour::ToVec3(playerRgb)};
}

} // namespace openblack::graphics
