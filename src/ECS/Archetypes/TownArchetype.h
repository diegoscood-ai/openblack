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
class TownArchetype
{
public:
	/// A new town, with its desire flags
	static entt::entity Create(int id, const glm::vec3& position, PlayerNames playerOwner, Tribe tribe);
	/// A town whose desire flags come from elsewhere (a loaded game's own records): none are made
	static entt::entity CreateWithoutDesireFlags(int id, const glm::vec3& position, PlayerNames playerOwner, Tribe tribe);
	TownArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
