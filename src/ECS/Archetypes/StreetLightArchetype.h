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

namespace openblack::ecs::archetypes
{
class StreetLightArchetype
{
public:
	/// A street light at a land script's position (CREATE_STREET_LIGHT): its place only, with no mesh, light or sound,
	/// not put on the map and taking no number of the objects' creation counter, as it is not an object
	static entt::entity Create(const glm::vec3& position);
	/// A street light as a saved game's load makes it again, at its saved place, with no stamp of its making (the
	/// loaded list keeps its place among the others)
	static entt::entity CreateLoaded(const glm::vec3& position);
	StreetLightArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
