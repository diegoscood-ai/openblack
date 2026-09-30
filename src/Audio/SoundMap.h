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

#include <glm/vec3.hpp>

namespace openblack::audio
{

/// GSoundMap::GetSurfaceType 0x71D8E0: the "surface" attribute of the .sad anim effect tables at a world position. 6
/// off the map (outside the cell grid or no land block there), 7 on a water cell (not MapCoords::IsLand 0x603720:
/// cell flag 0x10 set), else the surfaceSound of the cell's terrain material (1..8; anything else 3).
[[nodiscard]] int32_t GetSurfaceType(glm::vec3 position);

} // namespace openblack::audio
