/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>

/// The draws an object's creation takes from the C runtime's random numbers (its mist's frame counter, a temple's leash
/// posts, a chimney's smoke, a fish farm's fish, a lantern's light, a reward chest's dust). In play each is drawn as the
/// object is made. A saved game's load makes its objects class by class, but the game takes those draws once every
/// object is read, object by object in the order their reads ended; while a load defers them, each waits here with the
/// object it belongs to and is drawn at the load's end in that order.
/// See docs/wiki/engine/random.md#the-creation-draws-of-a-load
namespace openblack::ecs::creation_draws
{

struct Queue
{
	struct Entry
	{
		/// The object the draw belongs to: the one the load keeps for its saved record
		entt::entity owner {entt::null};
		std::function<void()> draw;
	};
	/// A load is under way: draws wait until EndLoad
	bool deferring {false};
	std::vector<Entry> entries;
};

/// Draws now, or, while a load defers, once the load ends
void Run(Queue& queue, entt::entity owner, std::function<void()> draw);

/// From here the creation draws wait for EndLoad
void BeginLoad(Queue& queue);

/// What a load's end took: the draws held, those of them whose owner had a rank, and those drawn
struct Taken
{
	size_t held {0};
	size_t ranked {0};
	size_t drawn {0};
};

/// Draws what waited, by the rank of each owner's record (the order its read ended), the draws of one owner in the order
/// they were made; an owner without a rank comes after all ranked ones, in the order its draws were made, and one no
/// longer alive draws nothing. Draws from then on run at once
Taken EndLoad(Queue& queue, const std::function<std::optional<size_t>(entt::entity)>& rankOf,
              const std::function<bool(entt::entity)>& alive);

/// Forgets what waits, and draws from then on run at once: for a load that stops on an error
void Drop(Queue& queue) noexcept;

} // namespace openblack::ecs::creation_draws
