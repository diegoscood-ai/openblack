/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class DeadTreeArchetype
{
public:
	/// A dead tree of that tree type made for `owner` (a script's names it, a dropped log is the neutral player's), with
	/// the given life and x, y, z angles
	static entt::entity Create(const glm::vec3& position, TreeInfo type, PlayerNames owner, float life, float xAngleRadians,
	                           float yAngleRadians, float zAngleRadians);
	DeadTreeArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
