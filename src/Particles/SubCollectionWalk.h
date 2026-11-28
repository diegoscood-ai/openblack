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

// The walk of an atom's sub-collections in an effect's step. Wiki: docs/wiki/magic/storm-and-tornado.md#openblack.

namespace openblack::particles
{

/// Updates each sub-collection an atom has as the walk begins, in its order. A rule run by one of them may add a
/// sibling under the same atom (the storm's cloud gathering adds the tornado group to its core): the game links a new
/// one at the head of the atom's list, which its walk has already passed, so the new one is first updated on the next
/// step. `subs` is the atom's list (indexable, its size read again as it may shrink), `update` takes one of its
/// elements dereferenced
template <class List, class Update>
void WalkSubCollections(List& subs, Update&& update)
{
	const std::size_t count = subs.size();
	for (std::size_t s = 0; s < count && s < subs.size(); ++s)
	{
		update(*subs[s]);
	}
}

} // namespace openblack::particles
