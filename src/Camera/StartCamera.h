/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <glm/gtx/polar_coordinates.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

/// Where the camera is when a game starts on a land. The land's script can give a start place for the camera, and the
/// camera then looks at it. A game that starts on a land whose script gives no place, or the place at the origin of
/// the map, starts with the camera over the player's temple, flying to it as the temple key's double tap flies. A
/// network game always starts that way. Without a temple the camera stays at the place of the script.
namespace openblack::start_camera
{

/// The camera a start makes looks at a point from this far, at these angles
inline constexpr float k_OriginDistance = 120.0f;
inline constexpr glm::vec2 k_OriginAngles {glm::radians(12.8571f), glm::radians(157.51f)};

/// Where the camera a start makes is, when it looks at a point
[[nodiscard]] inline glm::vec3 OriginOver(glm::vec3 focus)
{
	return focus + (k_OriginDistance * glm::euclidean(k_OriginAngles));
}

/// How a game starts on its first land
struct GameStart
{
	/// The game is a network game
	bool networkGame {false};
	/// The start place the land's script gave the camera, in map units; all zero when it gave none
	map_coords::MapCoords scriptPlace {};
};

/// Whether the camera starts at the player's temple and flies to it, when the player has a temple
[[nodiscard]] constexpr bool StartsAtTemple(const GameStart& start) noexcept
{
	return start.networkGame || start.scriptPlace == map_coords::MapCoords {};
}

} // namespace openblack::start_camera
