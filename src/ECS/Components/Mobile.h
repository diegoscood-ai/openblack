/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"

namespace openblack::ecs::components
{

struct Mobile
{
	char dummy;
};

struct MobileStatic
{
	MobileStaticInfo type; ///< This is 32 bits but could be 8 bits if stored in uint8_t
	/// A rock's player: the one it was made for (a rock from a player's reward chest; the halves of a split rock keep
	/// it), else the neutral player, as every other mobile static is
	PlayerNames owner {PlayerNames::NEUTRAL};
};

/// A mobile static's built percentage once a script's BUILT_PERCENTAGE has set it; without it, 0, as every static is
/// made (made at the first write, so that a land with no such script keeps no storage)
struct StaticBuiltPercentage
{
	float percent {0.0f};
};

struct MobileObject
{
	MobileObjectInfo type; ///< This is 32 bits but could be 8 bits if stored in uint8_t
};

} // namespace openblack::ecs::components
