/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalRules.h"

#include <cstdlib>

#include "3D/MapCoords.h"
#include "3D/ObjectMatrix.h"

using namespace openblack;

glm::vec3 animals::DeadBodySmokePlace(const glm::vec3& position, float heading, float height)
{
	// Half the height along the heading, turned as the body's own matrix turns it, its cosine and sine kept as floats
	const float out = height * 0.5f;
	const auto along = affine::AngleY(heading)[0];
	return position + glm::vec3(along.x * out, 0.0f, along.z * out);
}

int32_t animals::GrazeSearchCells(uint16_t domainRadius)
{
	// The tenth is a float, so a radius of 15 goes round 2 cells, not 1
	const float tenth = static_cast<float>(domainRadius) / 10.0f;
	return map_coords::FtoL(tenth * tenth);
}

bool animals::FarEnoughFromPredator(float distance, float minimum)
{
	return distance >= minimum;
}

bool animals::MayWeighAnotherReaction(uint8_t currentType, uint32_t currentId, uint8_t newType, uint32_t newId,
                                      bool sameTypeSwitch)
{
	if (newType != currentType)
	{
		return true;
	}
	return sameTypeSwitch && newId != currentId;
}

uint16_t animals::LookTowards(uint16_t current, uint16_t target, int32_t step)
{
	const int32_t difference = static_cast<int32_t>(target) - static_cast<int32_t>(current);
	if (std::abs(difference) < step)
	{
		return target;
	}
	// the short way: up for less than half a circle above, or more than half a circle below
	const bool up = difference > 0 ? difference < 0x400 : -difference >= 0x400;
	return static_cast<uint16_t>((static_cast<int32_t>(current) + (up ? step : -step)) & 0x7FF);
}
