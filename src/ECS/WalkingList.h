/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <vector>

namespace openblack::ecs::walking_list
{
/// A thing that starts walking a path goes to the head of the list, once: one already on it keeps its place
template <typename T>
void Enlist(std::vector<T>& list, T thing)
{
	if (std::ranges::find(list, thing) == list.end())
	{
		list.insert(list.begin(), thing);
	}
}
} // namespace openblack::ecs::walking_list
