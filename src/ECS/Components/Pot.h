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

#include "Enums.h"

namespace openblack::ecs::components
{

struct Pot
{
	uint16_t amount;
	uint16_t maxAmount;
	PotInfo type = PotInfo::_COUNT;
};

// PileResource sink offset (+0x84..+0xB0): piles rise out of / sink into the ground instead of scaling. A change of
// amount starts a 1 s move to the new target, a quartic with zero end velocity and acceleration.
struct PileSink
{
	float baseY;
	float height = 1.0f;
	float offset = 0.0f;
	float velocity = 0.0f;
	float target = 0.0f;
	float startOffset = 0.0f;
	float startVelocity = 0.0f;
	float time = 1.0f;
	float c2 = 0.0f; // coefficients of t^2/2, t^3/6, t^4/24
	float c3 = 0.0f;
	float c4 = 0.0f;
};

// Texture V offset of the object (LH3DObject vfunc 0xE8). PileFood::Draw scrolls the grain of the storage pit and
// magic food piles by 0.25 * sink / height, so that the grain stays put in the world and the pile seems to shrink.
struct UvScroll
{
	float v = 0.0f;
};

} // namespace openblack::ecs::components
