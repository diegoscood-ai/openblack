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

#include <algorithm>
#include <iterator>
#include <list>

namespace openblack::ecs
{

/// The order and pass rules of the game's dead list, with no registry: the things marked to be deleted wait on it and
/// are freed by later passes. `Thing` is any id (an entity in the game, a number in the tests).
///
/// - A thing marked goes to the head, not yet through a pass; a thing already on the list is not added again.
/// - A pass walks from the head (the last marked first). A thing not yet through a pass is only noted; a thing noted
///   by an earlier pass is taken off the list and freed. The next thing is taken before each one is handled, so a
///   thing marked while another is freed goes to the head and waits for the next pass.
/// - A draining pass (closing the game, clearing the map, loading a game) frees every thing it meets, and walks again
///   until the list is empty, so the things marked while freeing go too.
///
/// Through the turn this means a thing marked before the turn's pass is noted by it and freed by the next turn's; a
/// thing marked after the pass waits one turn more.
template <typename Thing>
class DeadList
{
public:
	/// Puts a thing just marked at the head, not yet through a pass. False when it was on the list already
	bool Mark(Thing thing)
	{
		if (Contains(thing))
		{
			return false;
		}
		_things.push_front(Entry {thing, false});
		return true;
	}

	/// One pass from the head. `free(thing)` is called on each thing freed, after it is off the list; it may mark other
	/// things. Returns how many were freed
	template <typename Free>
	std::size_t Process(bool drain, Free&& free)
	{
		std::size_t freed = 0;
		do
		{
			for (auto it = _things.begin(); it != _things.end();)
			{
				const auto next = std::next(it);
				if (!it->passed && !drain)
				{
					it->passed = true;
				}
				else
				{
					const auto thing = it->thing;
					_things.erase(it);
					++freed;
					free(thing);
				}
				it = next;
			}
		} while (drain && !_things.empty());
		return freed;
	}

	[[nodiscard]] bool Contains(const Thing& thing) const
	{
		return std::ranges::any_of(_things, [&thing](const Entry& entry) { return entry.thing == thing; });
	}

	/// Whether a thing on the list has been noted by a pass (false when it is not on the list)
	[[nodiscard]] bool Passed(const Thing& thing) const
	{
		const auto it = std::ranges::find_if(_things, [&thing](const Entry& entry) { return entry.thing == thing; });
		return it != _things.end() && it->passed;
	}

	[[nodiscard]] std::size_t Size() const { return _things.size(); }
	[[nodiscard]] bool Empty() const { return _things.empty(); }

	/// The things from the head
	template <typename Visit>
	void Each(Visit&& visit) const
	{
		for (const auto& entry : _things)
		{
			visit(entry.thing);
		}
	}

private:
	struct Entry
	{
		Thing thing;
		bool passed; ///< noted by a pass
	};

	// a linked list, as the original: marking at the head and freeing in the middle leave the walk's next valid
	std::list<Entry> _things;
};

} // namespace openblack::ecs
