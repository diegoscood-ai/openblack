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

#include <glm/vec2.hpp>

/// The creature room's waterfall, whose texture slides down it
namespace openblack::temple_waterfall
{

/// How much of a whole slide the texture moves each millisecond
constexpr float k_SlidePerMillisecond = 2.1e-5f;
/// The texture slides down ten of itself over one whole slide
constexpr float k_SlideLength = -10.0f;

/// How far through its slide the texture is after a frame the room is drawn: it moves on by the frame's whole
/// milliseconds, keeping only the part past the last whole slide
[[nodiscard]] constexpr float Advance(float slide, uint32_t frameMilliseconds) noexcept
{
	const float moved = (static_cast<float>(frameMilliseconds) * k_SlidePerMillisecond) + slide;
	return moved - static_cast<float>(static_cast<int32_t>(moved));
}

/// The texture's offset for a slide: down the waterfall only
[[nodiscard]] constexpr glm::vec2 Offset(float slide) noexcept
{
	return {0.0f, slide * k_SlideLength};
}

} // namespace openblack::temple_waterfall
