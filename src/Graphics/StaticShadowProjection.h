/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

/// The shadows of trees, rocks, buildings and the island's other features on the land. Black & White bakes them into
/// its land textures: it casts each object's triangles from a fixed sun onto the flat plane through the object's
/// origin, wherever the land under it lies, through its textures' masks. The shadows cover a texel's eight coverage
/// samples together, so where shadows overlap the land is no darker, and a fully covered texel is half as bright.
///
/// This is a projection through the sun's position, kept beside our bake for comparison and not used by it: our bake
/// (vs_static_shadow_instanced) slides each point by exactly its height along x and z, which this projection only
/// approaches, by a few ten-thousandths of the height at the heights of trees and buildings.
namespace openblack::graphics::static_shadow_projection
{

/// The game's default sun, from the object's origin: so far away that its shadows all fall the same way, as long
/// along the land's x and z as the object is high
inline constexpr glm::vec3 k_Sun {-500000.0f, 500000.0f, -500000.0f};
/// Under a fully covered texel the land's colour is multiplied by (255 - 255 / 2) / 256
inline constexpr float k_MaxDarkness = 0.5f;

/// Where a point of an object with its origin at origin falls on the plane through the origin. Points under the
/// origin fall straight down.
[[nodiscard]] glm::vec3 Project(glm::vec3 point, glm::vec3 origin);

} // namespace openblack::graphics::static_shadow_projection
