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
#include <span>

#include <glm/vec3.hpp>

#include "Body.h"

/// The shapes bodies are built from: a model's own points and triangles, or the hand-made shapes of trees, villagers and
/// animals and building pieces.
namespace openblack::physics::shapes
{

/// A body from a model's points and triangles: every vertex a point (about the mean of the points, scaled), every
/// triangle whose three vertices exist a face, the centre of mass at the mean of the points and the radius the farthest
/// scaled point from it
[[nodiscard]] Shape FromModel(std::span<const glm::vec3> positions, std::span<const std::array<uint32_t, 3>> triangles,
                              float scale);

/// A tree's body: 16 points (3 rings of 4, the top, and the bottom point three times) and 24 faces along the trunk;
/// rooted trees have their centre of mass at 0.4 H, dead ones at 0.5 H. Height and radius are scaled.
[[nodiscard]] Shape Tree(float height, float radius, float scale, bool rooted);

enum class Living : uint8_t
{
	Villager,
	Animal,
};
/// A villager's or an animal's body: 12 points on three levels (head, waist, feet) turned by pi/2 about Y, 20 faces,
/// centre of mass at half height, drag x 2. Height and radius are scaled.
[[nodiscard]] Shape LivingBody(Living kind, float height, float radius, float scale);

/// A building piece: its distinct vertices and a copy of each 0.45 behind along its normal (a slab), no faces (nothing
/// is hit by a piece), about the piece's origin; drag x 2
[[nodiscard]] Shape Fragment(std::span<const glm::vec3> points, std::span<const glm::vec3> normals);

} // namespace openblack::physics::shapes
