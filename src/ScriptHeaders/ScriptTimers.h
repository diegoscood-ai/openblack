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

/// The scripts' timers: a timer counts game turns from when it was set and holds how many it was set for
namespace openblack::script::timers
{

/// A timer as it was last set: the turn it was set on and how many turns it was set for
struct Timer
{
	uint32_t setTurn {0};
	int32_t turns {0};
};

} // namespace openblack::script::timers
