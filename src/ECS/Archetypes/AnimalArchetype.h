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
class AnimalArchetype
{
public:
	/// fn_00419D10: age 0 means GameRand(20) + 5. Returns entt::null for the flying animals, not created yet.
	static entt::entity Create(const glm::vec3& position, AnimalInfo type, int32_t flock, uint32_t age);
	AnimalArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
