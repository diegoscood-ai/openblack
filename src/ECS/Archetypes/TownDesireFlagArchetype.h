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
	/// drawn with the desire's own mesh. Not drawn until its store stands built and the town wants the thing enough
	static entt::entity Create(entt::entity town, TownDesireInfo desire, uint32_t point, MeshId mesh);
	TownDesireFlagArchetype() = delete;
};
} // namespace openblack::ecs::archetypes
