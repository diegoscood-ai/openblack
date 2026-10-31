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

#include <vector>

#include <glm/vec2.hpp>

/// The ring of light round a fight's arena: while a fight is on in it, a sheet of light stands on the land round its
/// edge. Wiki: docs/wiki/creature/fights.md#fights-the-arenas-ring.
namespace openblack::creature_arena_ring
{

/// Its points round the circle; the last closes the ring on the first
inline constexpr uint32_t k_Points = 32;
/// White light
inline constexpr uint32_t k_Rgb = 0xFFFFFF;
/// It stands a sixth of the radius high (the game's float for a sixth), its top spread out by a hundredth
inline constexpr float k_HeightShare = 0.16666667f;
inline constexpr float k_Spread = 1.01f;
/// Its strength moves one point along this often, and is fed in at full
inline constexpr float k_ShiftSeconds = 0.03f;
inline constexpr float k_Strength = 1.0f;

/// How high it stands round an arena of a radius
[[nodiscard]] float Height(float radius);
/// The points round an arena's middle (x, z) on its radius, worked as the game's floating point does: the angle of
/// each a whole turn's share of its place among one fewer than the points, its cosine stored before it is used
[[nodiscard]] std::vector<glm::vec2> Points(glm::vec2 centre, float radius, uint32_t count = k_Points);

} // namespace openblack::creature_arena_ring
