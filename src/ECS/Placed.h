/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs
{
class Registry;

/// Where a thing was last put (components::PlacedAt): every thing with a position is put where it is made, and again
/// where a script's SET_POSITION puts it and where its flight in the physics ends. Whether it has moved since asks
/// that place, as the game asks every thing that is not living of the place it was last put at.
namespace placed
{
/// Whether a thing at a position has moved from where it was put: compared as the map's whole coordinates, x and z only
[[nodiscard]] bool MovedFrom(glm::vec3 placedAt, glm::vec3 position);
} // namespace placed

/// Puts a thing where it is now; nothing for a thing with no position
void PlaceWhereItIs(Registry& registry, entt::entity entity);
/// Whether a thing is not where it was last put; a thing with no position, or never put, has not moved
[[nodiscard]] bool MovedSincePlaced(const Registry& registry, entt::entity entity);
} // namespace openblack::ecs
