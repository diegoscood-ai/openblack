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

/// On a god's player entity: the camera its interface reports, as its citadel set it when the citadel was made, looking
/// at the citadel's place from above it. It stays when the citadel goes. A computer god's hand moves where it is from
/// (ComputerHand::reportedCamera), never what it looks at.
struct GodReportedCamera
{
	glm::vec3 origin {0.0f};
	glm::vec3 focus {0.0f};
};

} // namespace openblack::ecs::components
