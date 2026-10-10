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

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>

/// How the miracles look outside their effects: here the one-shot globe's glint and the rings round an extreme miracle.
/// Pure rules. Nothing in the game reads them yet: the globes, the seeds inside them and the bands are drawn by our
/// own modules (magic::one_off, worship::seed_graphic, graphics::frame_anim), which these are checked against
/// (test_miracle_visuals). The phials, the hand's bands and its glow come with their first users.
namespace openblack::magic::visuals
{

// The globe

/// The glint on the globe's dome runs through the 16 cells of a 4 by 4 sheet, 18 a second, round and round
inline constexpr float k_GlintFrameRate = 18.0f;
inline constexpr int k_GlintCells = 16;
/// The globe's tint alpha, 150 of 255. The draw multiplies it with the white the globe is lit with first
/// ((255 x 150) >> 8), so the globe is added over what is behind it at 149, and its rings take that 149
inline constexpr uint8_t k_GlobeAlpha = 150;

/// The glint's frame after some seconds more
[[nodiscard]] float StepGlint(float frame, float seconds);
/// The corner of the glint's cell in the sheet: a quarter across for each of its column and row
[[nodiscard]] glm::vec2 GlintUvOffset(float frame);

// The rings round an extreme miracle in a globe or above a worship icon

/// Their alpha is 60 out of 256 of what they are drawn in
inline constexpr uint8_t k_RingAlpha = 60;

/// One ring for the first power-up, two for the second; none for a plain miracle
[[nodiscard]] int RingCount(int powerUp);
/// A ring's alpha in something drawn at the given alpha: 34 in a globe (149), 59 above an icon (255)
[[nodiscard]] uint8_t RingAlpha(uint8_t drawnAlpha);
/// The ring's turn before the camera's: laid flat, turned about the vertical by the spin (and half a radian more for the
/// second ring), tipped by 0.3, turned back or on by one radian, and tipped by 0.2 more
[[nodiscard]] glm::mat3 RingTurn(int ring, float spin);

} // namespace openblack::magic::visuals
