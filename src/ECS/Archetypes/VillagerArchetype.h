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
	/// @param joinTown kept for the callers: making a villager gives it no town or home; the map
	/// script's CREATE_VILLAGER_POS handler adds it to its town (town_villagers::AddVillagerToTown)
	static entt::entity Create(const glm::vec3& abodePosition, const glm::vec3& position, VillagerInfo type, uint32_t age,
	                           bool joinTown = true);
	/// A loaded game's villager, as the game's load makes one: of its row, at its place, angle and size, a child's
	/// meshes or a grown up's, with no random draw and no creation number; the load then puts its saved values in and
	/// places it (PlaceRestored)
	static entt::entity Restore(const glm::vec3& position, VillagerInfo type, float headingRadians, float scale, bool child);
	/// A restored villager put in the map and given its first clip, in the states its load gave it
	static void PlaceRestored(entt::entity villager);
	VillagerArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
