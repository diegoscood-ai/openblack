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

#include <array>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

/// The main room's pool: its water drawn twice over itself, shimmering on a clock of its own
namespace openblack::temple_pool
{

/// The water is the temple's light darkened to 208 of 256 in each byte
constexpr uint32_t k_WaterShare = 208;
/// The second layer stands this much above the first, turned an eighth of a turn
constexpr float k_SecondLayerLift = 0.05f;
constexpr float k_SecondLayerTurn = 0.7853982f;

/// One of the two layers the pool's water is drawn in
struct Layer
{
	glm::vec3 position {0.0f};
	float yaw {0.0f};
	int32_t alpha {0};
	glm::vec2 uvOffset {0.0f};
};

/// The pool's clock after a frame of whole milliseconds: their thousandth, a float, added to it
[[nodiscard]] float Advance(float seconds, int32_t frameMilliseconds);
/// The first layer's alpha at a time on the pool's clock: swelling and ebbing with its sine, from 96 to 160, truncated
[[nodiscard]] int32_t Alpha(float seconds);
/// The water's colour in bytes, from the temple's light in bytes: each times 208 over 256, rounded down
[[nodiscard]] glm::vec3 WaterColour(glm::vec3 lightBytes);
/// The two layers at a time on the pool's clock: the first where the pool is, the second lifted and turned with the
/// alpha the first lacks, their textures sliding apart
[[nodiscard]] std::array<Layer, 2> Layers(float seconds);

} // namespace openblack::temple_pool
