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

namespace openblack::ecs::components
{

/// Where a tree of a forest (Tree::forestId) stands in the forest's two lists
struct ForestMember
{
	/// Among the forest's growing trees rather than its grown ones. A tree joins as a growing one when made to grow and
	/// still short of its largest size, and leaves them for the grown ones, for good, when its clock runs out with it
	/// full grown.
	bool growing {false};
	/// When it joined the list it is on: of trees as far from the forest's place, the earlier is met first
	uint32_t listed {0};
};

} // namespace openblack::ecs::components
