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
	/// The reaction turn it last took up a thing flying through the air, kept from reaction to reaction: another catches
	/// its eye only once long enough has gone by
	std::optional<uint32_t> sawFlying;
};

/// A creature a script has told to take no notice of reactions, until a script tells it to again. Every creature starts
/// taking notice
struct CreatureReactionsOff
{
};

} // namespace openblack::ecs::components
