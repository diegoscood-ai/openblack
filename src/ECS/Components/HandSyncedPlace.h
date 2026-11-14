/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// The place on the land the player's hand stands for, as its last hand sync gave it. A reaction the hand starts (its
/// pick-up) is placed there, not where the hand is drawn.
struct HandSyncedPlace
{
	map_coords::MapCoords at {};
};

} // namespace openblack::ecs::components
