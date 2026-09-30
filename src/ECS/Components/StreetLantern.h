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

/// GStreetLantern (StreetLantern.cpp, GStreetLantern::Create 0x7346E0): a lantern of the map script or of CHL CREATE.
/// +0x58 = (info != GMobileStaticInfo[7]): a country lantern (MSH_B_CAMPFIRE) rather than a town one (MSH_O_TOWNLIGHT).
struct StreetLantern
{
	bool country {false};
};

/// A village light of fn_00823240(pos, type) (night_lights): the flames, the glow and the light stamped on the land.
/// type 0 = town light (5 units up), 1 = country lantern (1 unit up). GStreetLantern keeps it in +0x5C; the two lamps
/// of the Norse Gate (AnimatedStatic 0x422300) have one each.
struct LanternLight
{
	uint8_t type {0};
};

} // namespace openblack::ecs::components
