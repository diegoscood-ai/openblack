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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The invisible stone a teleport miracle leaves on the land, with its swirling pool. The player's stones form a
/// network the player's villagers and creature jump between.
struct TeleportStone
{
	/// Someone who turned aside into the stone, and where they were going
	struct Traveller
	{
		entt::entity living {entt::null};
		glm::vec3 destination {0.0f}; ///< Map coordinates as metres (y above the land)
	};

	/// The spell's player; a stone whose spell has none is in no player's network and spreads no reaction
	std::optional<PlayerNames> player;
	/// The miracle it belongs to; it goes with it
	entt::entity spell {entt::null};
	/// The reaction it spreads round itself, which the passers-by react to
	uint32_t reaction {0};
	/// Its pool's particle effect, 0 none
	uint32_t pool {0};
	/// The order the stones were made in: the newest are first in their player's list
	uint32_t serial {0};
	/// The newest first
	std::vector<Traveller> travellers;
};

} // namespace openblack::ecs::components
