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

/// On a player: the value a script's SET_PLAYER_WIND_RESISTANCE gave, as it was given. Anything but 0 and what the
/// player's hand throws flies without drag
struct NoWindResistance
{
	uint32_t value {0};
};

} // namespace openblack::ecs::components
