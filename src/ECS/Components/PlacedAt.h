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

namespace openblack::ecs::components
{
/// Where a thing was last put: where it was made (every thing with a position is given it then), where a script's
/// SET_POSITION put it, or where its flight in the physics ended. A thing that is not there now (x or z, on the map's
/// grid) has moved since: every other move, a walk, a push into place or a carry, leaves it as it was
struct PlacedAt
{
	glm::vec3 position {0.0f};
};
} // namespace openblack::ecs::components
