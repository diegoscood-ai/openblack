/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>

#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "Graphics/ArgbColour.h"
#include "Magic/Hand/HandMagicFX.h"

/// The glow over a hand holding a miracle as one draw for the renderer (Renderer::DrawHandGlow): the hand's own mesh,
/// posed and shaped as the hand is, drawn once more over itself right after it, every primitive added (mode 13) with
/// the flow's texture in place of its skin, one cell of the flow's 8 x 4 sheet at a time, in the player's colour at
/// the glow's strength, lit as the original's animated models are and culled one-sided. Wiki:
/// docs/wiki/magic/hand-casting.md#the-hand.
namespace openblack::graphics
{

/// The glow is drawn only while its strength is above this
inline constexpr float k_HandGlowShown = 0.01f;
/// The flow's sheet: 8 cells across, each an eighth of the sheet across and down
inline constexpr int k_HandGlowSheetColumns = 8;
inline constexpr float k_HandGlowCellSize = 0.125f;

/// One draw of the hand's glow
struct HandGlowDraw
{
	/// The glow's strength as the alpha of the hand's colour while it is drawn: 255 times it with the fraction dropped
	/// (204 for 0.8)
	uint8_t alpha {0};
	/// The cell of the flow's sheet this frame, column + 8 x row
	uint8_t cell {0};
	/// The player's colour, 0x00RRGGBB
	uint32_t rgb {0};
};

/// The cell of the flow's sheet a texture offset of whole eighths shows (magic::hand_fx::Glow::uvOffset)
[[nodiscard]] inline uint8_t HandGlowCell(glm::vec2 uvOffset) noexcept
{
	const auto column = static_cast<int>(std::lround(uvOffset.x / k_HandGlowCellSize));
	const auto row = static_cast<int>(std::lround(uvOffset.y / k_HandGlowCellSize));
	return static_cast<uint8_t>(column + (k_HandGlowSheetColumns * row));
}

/// The draw of the hand's glow in a player's colour (0xAARRGGBB, its alpha not read), or none while the glow's strength
/// is a hundredth or less (nothing in the hand)
[[nodiscard]] inline std::optional<HandGlowDraw> HandGlowOf(const magic::hand_fx::Glow& glow, uint32_t playerArgb) noexcept
{
	if (glow.alpha <= k_HandGlowShown)
	{
		return std::nullopt;
	}
	constexpr float k_ByteMax = 255.0f;
	return HandGlowDraw {
	    .alpha = static_cast<uint8_t>(glow.alpha * k_ByteMax),
	    .cell = HandGlowCell(glow.uvOffset),
	    .rgb = playerArgb & 0x00FFFFFFu,
	};
}

/// The glow's instance row (the five columns vs_object.sc reads) from the hand's: the hand's matrix, texture offset,
/// specular and flags as they are, its colour the player's in place of the land's light, and its alpha the glow's in
/// place of the hand's own
[[nodiscard]] inline std::array<glm::vec4, 5> HandGlowRow(const glm::mat4& handRow, const glm::vec4& handColours,
                                                          const HandGlowDraw& glow) noexcept
{
	constexpr float k_ByteMax = 255.0f;
	std::array<glm::vec4, 5> row {handRow[0], handRow[1], handRow[2], handRow[3], handColours};
	// the first column's w is how far the object has faded, 1 - its alpha
	row[0].w = 1.0f - (static_cast<float>(glow.alpha) / k_ByteMax);
	argb_colour::PackInstanceColour(row[4], glow.rgb);
	return row;
}

/// What vs_object.sc's u_window.z carries for the glow: 1 + the cell; 0 for every other draw
[[nodiscard]] constexpr float HandGlowWindowZ(std::optional<uint8_t> cell) noexcept
{
	return cell.has_value() ? 1.0f + static_cast<float>(*cell) : 0.0f;
}

} // namespace openblack::graphics
