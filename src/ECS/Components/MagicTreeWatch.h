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

/// A villager bewildered by the trees of a forest miracle: the number of the next tree it looks at, in the forest's
/// order, and how many trees it has looked at. The tree it looks at now is its reaction's object
struct MagicTreeWatch
{
	uint32_t next {0};
	uint32_t looked {0};
};

} // namespace openblack::ecs::components
