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

/// What the ground of a map cell is made of, as the forest miracle and the Help key ask it
namespace openblack::ecs::terrain_material
{

/// The terrain material of a cell: 27 under deep snow (snow >= 27 at the cell's corner), else the land's material (the
/// cell's country, the second material of its altitude, that material's type; 0 off the map or without a block) with
/// 0 -> 1
[[nodiscard]] uint32_t AtCell(glm::u16vec2 cell);
/// The cell is on the map (both coordinates at most 0x1FF and within the island) and in one of the land's blocks
[[nodiscard]] bool HasLandBlock(glm::u16vec2 cell);

} // namespace openblack::ecs::terrain_material
