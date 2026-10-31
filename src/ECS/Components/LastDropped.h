/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

namespace openblack::ecs::components
{

/// On a player's entity: the last thing the player's hand let go of, one for the player's interface. The player's next
/// drop of anything replaces it, and another hand's drop of the same thing takes it away (ecs::last_dropped). Given at
/// the player's first throw or drop
struct LastDropped
{
	entt::entity object {entt::null};
};

} // namespace openblack::ecs::components
