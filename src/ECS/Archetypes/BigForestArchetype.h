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
class BigForestArchetype
{
public:
	/// A new big forest: it makes its own forest and holds its info's wood at its scale
	static entt::entity Create(const glm::vec3& position, BigForestInfo type, uint32_t unknown, float yAngleRadians,
	                           float scale);
	/// A big forest of a forest that is there already (its key in the forest list), holding that wood
	static entt::entity Create(const glm::vec3& position, BigForestInfo type, float yAngleRadians, float scale,
	                           uint32_t forestId, float wood);
	BigForestArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
