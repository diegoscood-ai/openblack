/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <iterator>
#include <span>
#include <vector>

#include <entt/entity/entity.hpp>

/// When the effects that the land's draw moves on take a step. The game moves some effects on in the draw of the thing
/// that has them, by the game time of the frame: such as the pool of a teleport stone, the swirl of a miracle dispenser,
/// the glints of a scroll and the symbols over a village centre.
/// Such an effect takes a step only in a frame that draws its thing, by that frame's game time. Its random numbers are
/// only this computer's, so its steps do not change the world.
namespace openblack::particles::drawn_steps
{

/// The order in which the frame draws some things, such as the stones or the dispensers. With a camera and a land the
/// frame draws the things of the land's list in the list's order, the nearest blocks first, then the things that are
/// not on the list's blocks. Without a camera or a land it draws every thing, in the given order.
/// `onBlocks(thing)` tells whether a thing is on the land's blocks, so that the list decides whether the frame draws it.
template <typename OnBlocks>
[[nodiscard]] std::vector<entt::entity> DrawOrder(std::span<const entt::entity> list, std::span<const entt::entity> things,
                                                  bool listDecides, OnBlocks&& onBlocks)
{
	if (!listDecides)
	{
		return {things.begin(), things.end()};
	}
	std::vector<entt::entity> order;
	order.reserve(things.size());
	std::ranges::copy_if(list, std::back_inserter(order),
	                     [things](entt::entity listed) { return std::ranges::find(things, listed) != things.end(); });
	std::ranges::copy_if(things, std::back_inserter(order), [&onBlocks](entt::entity thing) { return !onBlocks(thing); });
	return order;
}

/// The things that the frame does not draw: the things that are not in the draw order. The effects of these things do
/// not show in the frame, because only the draw of a thing shows its effects.
[[nodiscard]] inline std::vector<entt::entity> LeftOut(std::span<const entt::entity> things,
                                                       std::span<const entt::entity> order)
{
	std::vector<entt::entity> left;
	std::ranges::copy_if(things, std::back_inserter(left),
	                     [order](entt::entity thing) { return std::ranges::find(order, thing) == order.end(); });
	return left;
}

} // namespace openblack::particles::drawn_steps
