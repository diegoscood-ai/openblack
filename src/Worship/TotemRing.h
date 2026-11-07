/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include <glm/vec3.hpp>

// The ring of light round a worship site's altar, as pure functions of the site: where it stands, how strong it is,
// its colour and how fast its stars slide (Worship/WorshipSite.cpp stands it and moves it on as the altar is drawn).
// Every product and sum is a single precision one, in the game's order; sines and cosines are taken in double and
// rounded once by the operation that uses them.

namespace openblack::worship::totem_ring
{

/// The ring: ten points on a circle round the middle of the site's dance, the last on the first, as high as its wave
/// lets it and its top spread out by half as much again
inline constexpr size_t k_Points = 10;
inline constexpr float k_Radius = 3.0f;
inline constexpr float k_WaveHeight = 10.0f;
inline constexpr float k_Spread = 1.5f;
/// How often the strength fed in at its first point moves one point along, in seconds
inline constexpr float k_ShiftSeconds = 0.03f;

/// The ring's points round the middle of the dance (a point on the land), point i at the angle i x 2 pi / 9
[[nodiscard]] std::array<glm::vec3, k_Points> Points(const glm::vec3& centre);

/// The strength fed in: how full the site's battery is, its stored prayer over the most it holds, kept to 0..1 (0 when
/// the share is not a number)
[[nodiscard]] float Strength(float stored, float most);

/// The ring's colour (0xRRGGBB): its player's colour, each of its red, green and blue scaled by the site's strain pulse
/// (0..1, cut to a whole 0..255) while the miracles ask more than the dancers give
[[nodiscard]] uint32_t Colour(uint32_t playerColour, bool overloaded, float pulse);

/// How fast the stars slide up the ring: down, as fast as the site is strained (its strain, unclamped)
[[nodiscard]] constexpr float SlideSpeed(float strain)
{
	return -strain;
}

} // namespace openblack::worship::totem_ring
