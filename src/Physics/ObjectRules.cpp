/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectRules.h"

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"

bool openblack::physics::objects::RockBreaksWhenTapped(float height)
{
	return height > 0.7f;
}

glm::vec3 openblack::physics::objects::PlacedOnTheMap(const LandIslandInterface* land, glm::vec3 point)
{
	// The land is read once, at the point on the grid: the height above it is kept, and the height is the land's there
	// plus that
	const glm::vec2 onGrid {map_coords::Quantise(point.x), map_coords::Quantise(point.z)};
	const float ground = land != nullptr ? land->GetHeightAt(onGrid) : 0.0f;
	const float above = point.y - ground;
	return {onGrid.x, ground + above, onGrid.y};
}
