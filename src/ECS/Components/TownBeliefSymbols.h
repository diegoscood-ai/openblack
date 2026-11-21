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

/// A town centre's belief symbols (particles::town_belief): the id of its own effect (in the particle manager), on the
/// town centre's entity from the first step that sees it, removed with the centre or when the effects
/// are cleared. 0 once the effect is gone
struct TownBeliefSymbols
{
	uint32_t effect {0};
};

} // namespace openblack::ecs::components
