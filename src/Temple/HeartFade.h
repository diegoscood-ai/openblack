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

#include "Graphics/RenderModes.h"

/// The colours a citadel heart is drawn with. Each time it is drawn, its land light is multiplied by its tint, white
/// with the heart's alpha, and its own specular colour is added to the land's. As it is destroyed it fades out from
/// fourteen seconds, drawn see-through once its alpha is low enough, and from three and a half seconds each draw gives
/// it its god's colour, by the local player's heartbeat, as the specular its next draw adds. Free of state, so it is
/// tested on its own; every step is worked in single precision, one rounding a step, as the game works it. Once it is
/// drawn see-through, the snow lying on it thins with its alpha.
namespace openblack::heart_fade
{

/// The heart's alpha while it is whole
inline constexpr uint8_t k_Whole = 0xFF;
/// A heart being destroyed is drawn see-through from the first draw whose alpha (DrawnAlpha) is at most this
inline constexpr uint8_t k_SeeThroughAlpha = 0xF0;
/// What the materials of a see-through heart's model take on: no depth written, back faces culled, not added, their
/// alpha kept. The model keeps them from then on
inline constexpr graphics::render_modes::MaterialProperties k_SeeThroughMaterial {
    .additive = false,
    .zWrite = false,
    .doubleSided = false,
    .change = true,
    .alpha = true,
};
/// From this clock of its destruction, in seconds, each draw gives the heart its god's colour as its specular ...
inline constexpr float k_SpecularFrom = 3.5f;
/// ... rising over this many seconds ...
inline constexpr float k_SpecularOver = 3.0f;
/// ... between these levels, out of 256, times the heartbeat's pulse on the way
inline constexpr float k_SpecularLow = 10.0f;
inline constexpr float k_SpecularHigh = 60.0f;

/// The tint the heart's land light is multiplied by: white, with the heart's alpha, 0xAARRGGBB
[[nodiscard]] constexpr uint32_t Tint(uint8_t alpha) noexcept
{
	return (static_cast<uint32_t>(alpha) << 24u) | 0x00FFFFFFu;
}

/// The alpha the heart is drawn with: the land light's, which is whole, times its tint's, over 256
[[nodiscard]] constexpr uint8_t DrawnAlpha(uint8_t alpha) noexcept
{
	return static_cast<uint8_t>((static_cast<uint32_t>(k_Whole) * alpha) >> 8u);
}

/// Whether a draw of the heart at that alpha turns it see-through: only while it is being destroyed
[[nodiscard]] constexpr bool TurnsSeeThrough(uint8_t alpha, bool destroying) noexcept
{
	return destroying && DrawnAlpha(alpha) <= k_SeeThroughAlpha;
}

/// How much of the snow lying under a see-through heart shows on it, 0 to 255, from the level the snow there gives it
/// and its drawn alpha: the level times the alpha's share above 128, out of 128, rounded down, none at or below 128 and
/// at most 255. Each draw works it out again from the snow under the heart, so it never builds up
[[nodiscard]] constexpr int32_t SnowLevel(int32_t level, uint8_t drawnAlpha) noexcept
{
	const int32_t product = (static_cast<int32_t>(drawnAlpha) - 0x80) * level;
	if (product < 0x80)
	{
		return 0;
	}
	if (product >= 0xFF * 0x80)
	{
		return 0xFF;
	}
	return product >> 7;
}

/// The specular colour a draw of a heart being destroyed gives it, for its next draw: none up to k_SpecularFrom (or for
/// a clock that is not a number). Then the share of k_SpecularOver past it (held to 0..1), times the pulse, sets a level
/// from k_SpecularLow up to k_SpecularHigh, truncated to a whole number and kept to its low byte; each of the god's
/// colour's red, green and blue times the level over 256, its alpha the god's colour's. `god` is 0xAARRGGBB
[[nodiscard]] std::optional<uint32_t> Specular(float clock, float pulse, uint32_t god);

} // namespace openblack::heart_fade
