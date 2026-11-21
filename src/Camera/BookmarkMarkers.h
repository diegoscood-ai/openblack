/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack
{
class LandIslandInterface;
}

/// Where and when the camera bookmarks' markers are drawn: once a frame that draws the land, while the bookmarks are on
/// and no script holds the wide screen, each bookmark that is set stands at the land point of its map position, raised
/// a little.
namespace openblack::bookmark_markers
{

/// How far above its land point a marker stands
inline constexpr float k_HeightAboveTheLand = 2.0f;

/// The map position's land point (the ground there plus the position's altitude), then k_HeightAboveTheLand added to
/// its height. Without an island the ground is 0
[[nodiscard]] glm::vec3 MarkerPosition(const LandIslandInterface* island, const map_coords::MapCoords& position);

/// The markers are drawn while the bookmarks are on (the scripts' switch) and no script holds the wide screen
[[nodiscard]] constexpr bool MarkersShown(bool bookmarksOn, bool scriptWideScreen)
{
	return bookmarksOn && !scriptWideScreen;
}

} // namespace openblack::bookmark_markers
