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

#include <array>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Worship/LeashPosts.h"

namespace openblack::ecs::components
{

/// One of a temple's three leash posts, on an entity of its own with a Transform at the post's point. The posts are
/// made with their temple's heart, destroyed with it and never saved (worship::temple_leash)
struct LeashPost
{
	/// Which post: 0, 1, 2 hang the aggression, learning and compassion leashes (worship::leash_posts::TypeOf)
	uint8_t index {0};
	/// The temple's player
	PlayerNames owner {PlayerNames::NEUTRAL};
	/// The temple heart it belongs to
	entt::entity heart {entt::null};
	/// The collar's scroll and turns and the smoke's frame clock, seeded by the post's four random draws when it is made
	worship::leash_posts::Spin spin;
};

/// On a temple's heart: its three leash posts and which of them is picked
struct TempleLeash
{
	/// The posts, in their order (LeashPost::index)
	std::array<entt::entity, worship::leash_posts::k_Count> posts {entt::null, entt::null, entt::null};
	/// The picked post, 0..2, or none (worship::leash_posts::k_NoPick) as the temple is made
	int32_t pick {worship::leash_posts::k_NoPick};
};

} // namespace openblack::ecs::components
