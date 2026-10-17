/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <vector>

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// The creatures a script has made this creature's friends (CREATURE_FORCE_FRIENDS), each once, the newest first as
/// in the original's list. Nothing reads it yet: in the original a friend cannot be attacked by the creature and
/// counts in its choice of activities, neither of which is ported
struct CreatureFriends
{
	std::vector<entt::entity> friends;
};

} // namespace openblack::ecs::components
