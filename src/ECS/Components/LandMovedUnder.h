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
/// A thing that stays where it stands when the land changes under it (a building, a tree, a static, a spell icon
/// going up) but keeps its height over the ground it was put on: its next placing (a scale, an angle, a growth) puts
/// it at that height over the new ground. Where it stood then, and the ground under it before the first change
struct LandMovedUnder
{
	glm::vec3 standing {0.0f};
	float groundBefore {0.0f};
};
} // namespace openblack::ecs::components
