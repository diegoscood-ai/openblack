/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/fwd.hpp>
#include <glm/fwd.hpp>

#include "Enums.h"

namespace openblack
{
enum class MeshId : uint32_t;
}

namespace openblack::ecs::archetypes
{
class TownDesireFlagArchetype
{
public:
	/// One of a town's flags over its store (town_desire_flags), for a desire, flying at the store's special point and
	/// drawn with the desire's own mesh, made at `place` until a store takes it, with the desire's object scale. It takes
	/// the next object creation number and goes into the map. Not drawn until its store stands built and the town wants
	/// the thing enough
	static entt::entity Create(entt::entity town, TownDesireInfo desire, uint32_t point, MeshId mesh, float scale,
	                           const glm::vec3& place);
	/// A town's flags, made as the town is made: one for each desire whose info gives it a place in the store, in the
	/// desires' order, each 5 m from the town at its place's share of the circle (town_desire_flags::StartPlace). The
	/// town keeps them (components::TownDesireFlags). None without the game's info
	static void CreateForTown(entt::entity town, const glm::vec3& townPosition);
	TownDesireFlagArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
