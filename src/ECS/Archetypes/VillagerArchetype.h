/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{
class VillagerArchetype
{
public:
	/// @param joinTown false for the villagers made by CHL CREATE: Villager::Create 0x74FBE0 gives them no town or home
	/// (the map script's CREATE_VILLAGER_POS handler is the one that houses them)
	static entt::entity Create(const glm::vec3& abodePosition, const glm::vec3& position, VillagerInfo type, uint32_t age,
	                           bool joinTown = true);
	VillagerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
