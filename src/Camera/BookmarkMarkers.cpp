/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BookmarkMarkers.h"

namespace openblack::bookmark_markers
{

glm::vec3 MarkerPosition(const LandIslandInterface* island, const map_coords::MapCoords& position)
{
	auto point = map_coords::ToWorld(island, position);
	point.y += k_HeightAboveTheLand;
	return point;
}

} // namespace openblack::bookmark_markers
