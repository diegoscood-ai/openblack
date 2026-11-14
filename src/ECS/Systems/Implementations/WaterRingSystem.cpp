/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "WaterRingSystem.h"

#include <algorithm>

#include "3D/LandLightFrame.h"

using namespace openblack;
using namespace openblack::ecs::systems;

bool WaterRingSystem::Add(const water_rings::Ring& ring)
{
	if (_rings.size() >= water_rings::k_MostRings)
	{
		return false;
	}
	auto& added = _rings.emplace_back(ring);
	// The colour is fixed now, and kept for the ring's life
	if (added.seaLight)
	{
		added.argb = (added.argb & 0xFF000000u) | FrameLandLight(255);
		added.seaLight = false;
	}
	return true;
}

void WaterRingSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	std::erase_if(_rings, [gameTime](water_rings::Ring& ring) { return !water_rings::Advance(ring, gameTime.count()); });
}
