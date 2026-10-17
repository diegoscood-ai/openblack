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

/// The reaction a creature follows, as a villager or an animal does: what it reacts to, until the reaction ends, the
/// thing goes, or its time is up. Given the first time the creature takes a reaction up (ECS/Effects/Reactions)
struct CreatureReaction
{
	/// The reaction's id (ECS/Effects/Reactions.h), 0 none, and its type, kept for when the reaction has gone
	uint32_t reaction {0};
	uint8_t type {0};
	/// What started it
	entt::entity object {entt::null};
	/// Among the reaction's followers, whom its end stops. A creature switching to another reaction leaves the old
	/// one's followers at once, before it takes the new one up
	bool follower {false};
};

} // namespace openblack::ecs::components
