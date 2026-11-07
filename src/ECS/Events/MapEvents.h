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

namespace openblack::ecs::events
{
/// A thing's collide data was let go of: it left the map's cells, or it goes back in at once to be made again (it moved,
/// turned or changed size on the map). Published once the thing is out of the cells, so a look over them no longer
/// meets it. The walkers holding one of its circles let go of it there and then
struct CollideDataReleased
{
	entt::entity thing {entt::null};
	/// For a thing of many cells: itself and every other such thing its circles overlap, through each other, found in
	/// the cells of each; empty for any other thing
	std::vector<entt::entity> overlapping {};
};
} // namespace openblack::ecs::events
