/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/TempleInteriorInterface.h"
#include "LandLoad.h"

/// What a land's load does with the temple the player may be inside.
/// docs/wiki/scripts/land-change-and-saving.md#load-map-inside-the-temple
namespace openblack::land_load
{

/// Before the land goes. A script's change of land leaves the player inside the temple, which lets go of what it held of
/// the land; every other load takes the player out of it first, rooms and all, and a room the game over kept them in
/// holds them no longer, so that leaving it ends nothing (a saved game loaded from the save game room)
inline void TempleBeforeLoad(TempleInteriorInterface& temple, Resets resets)
{
	if (resets.keepsTemple && temple.Active())
	{
		temple.BeginLandChange();
		return;
	}
	temple.ForgetKeptRoom();
	if (temple.Active())
	{
		temple.Deactivate();
	}
}

/// Once the next land is loaded: the temple the player stayed in sets its rooms up again, the world still paused
inline void TempleAfterLoad(TempleInteriorInterface& temple, Resets resets)
{
	if (resets.keepsTemple && temple.Active())
	{
		temple.EndLandChange();
	}
}

} // namespace openblack::land_load
