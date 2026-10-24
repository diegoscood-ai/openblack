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

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{
/// What a missionary disciple preaches through: a thing standing where the missionary is, the initiator of its
/// REACTION 39 (ReactToMissionary) of the missionary's own player, in the list of the town it preaches to
/// (Town::missionaries). It is made at the map's origin; from its first turn on its Transform follows the missionary
/// (villager::ProcessMissionary)
struct MissionaryControl
{
	entt::entity missionary {entt::null};
	/// The reaction's id (ecs::effects::reactions), 0 for none
	uint32_t reaction {0};
};
} // namespace openblack::ecs::components
