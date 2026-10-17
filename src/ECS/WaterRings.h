/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/WaterRingSystemInterface.h"
#include "Locator.h"

// The old names of the water rings, forwarding to Locator::waterRingSystem. It stays until the physics files move to
// the water ring system (they are held by another change); nothing else uses it.
namespace openblack::ecs
{

using WaterRing = water_rings::Ring;

/// Adds a ring to the water ring system; false when it is full
inline bool AddWaterRing(const WaterRing& ring)
{
	return Locator::waterRingSystem::value().Add(ring);
}

} // namespace openblack::ecs
