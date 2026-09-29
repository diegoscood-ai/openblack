/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/fwd.hpp>

namespace openblack::ecs
{

/// Moves the fish of every fish farm shoal (fn_00824DA0 per shoal, fn_008248E0 per fish) by `seconds` of game time
/// and works out each shoal's visibility and alpha from the camera distance
void UpdateFishShoals(float seconds, const glm::vec3& camera);

} // namespace openblack::ecs
