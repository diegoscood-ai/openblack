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

#include <bit>
#include <span>

#include <glm/vec3.hpp>

/// How far ahead of the world camera the land is, as a press of the mouse finds it. The turns, the zoom and the single
/// tap of the temple key use this distance.
namespace openblack::click_distance
{

/// The number of rays down the middle of the screen
inline constexpr int32_t k_Rays = 16;
/// The distance that counts as one more hit, so that the mean is this when no ray hits the land
inline constexpr float k_NoHitDistance = 50.0f;
/// How much each unit of the pitch moves the distance towards the focus distance: the float of 1 / (6 pi), one product
/// where a division by pi and by 6 would round twice
inline constexpr float k_PitchShare = std::bit_cast<float>(0x3D594CAFu);

/// The distance from the camera to the point where a ray hits the land: the squares add z first, then y, then x
[[nodiscard]] float HitDistance(const glm::vec3& origin, const glm::vec3& hit);
/// The harmonic mean of the distances of the hits, with one more hit at the no-hit distance, summed in the hits' order
[[nodiscard]] float MeanDistance(std::span<const float> hitDistances);
/// The mean distance moved towards the focus distance, more as the camera looks more steeply down: the pitch from the
/// origin to the focus times k_PitchShare, kept between 0 and 1 (0 for no number)
[[nodiscard]] float TowardsFocus(float meanDistance, float focusDistance, const glm::vec3& origin, const glm::vec3& focus);

} // namespace openblack::click_distance
