/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalRules.h"

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
