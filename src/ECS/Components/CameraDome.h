/******************************************************************************
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

/// The dome over a creature or a temple that keeps the player's camera out (camera_zones::Exclusion of the dome kind):
/// where its base's middle is, how far it reaches across the ground and how high it rises over its base. Made and moved
/// by its thing (ecs::camera_domes), gone with it; never saved
struct CameraDome
{
	glm::vec3 position {0.0f};
	float radius {0.0f};
	float height {0.0f};
};

} // namespace openblack::ecs::components
