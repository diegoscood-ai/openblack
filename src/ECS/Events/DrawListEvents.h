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

namespace openblack::ecs::events
{
/// Something asks for the object draw list to be rebuilt on the next frames: the number of rebuilds still asked for
/// becomes `count`. It is a store, not an addition, so the last request wins (a 2 then a 1 leaves 1)
struct DrawListRebuildRequested
{
	uint8_t count {1};
};
} // namespace openblack::ecs::events
