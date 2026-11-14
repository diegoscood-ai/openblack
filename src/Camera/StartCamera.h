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

#include <bit>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Camera/ScriptCamera.h"

/// Where the camera is when a game starts on a land. The land's script can give a start place for the camera, and the
/// camera then looks at it. A game that starts on a land whose script gives no place, or the place at the origin of
/// the map, starts with the camera over the player's temple, flying to it as the temple key's double tap flies. A
/// network game always starts that way. Without a temple the camera stays at the place of the script.
namespace openblack::start_camera
{

/// The camera a start makes looks at a point from this far, from this heading (2.749) and at this pitch (a fourteenth
/// of a half turn), the game's own floats
inline constexpr float k_OriginDistance = 120.0f;
inline constexpr float k_OriginHeading = std::bit_cast<float>(uint32_t {0x402FEF9E});
inline constexpr float k_OriginPitch = std::bit_cast<float>(uint32_t {0x3E65C8FA});

/// Where the camera a start makes is, when it looks at a point: the point from the distance, the heading and the pitch,
/// as the game places a point from them
[[nodiscard]] inline glm::vec3 OriginOver(glm::vec3 focus)
{
	return script_camera::PointFromDistanceHeadingAndPitch(focus, k_OriginDistance, k_OriginHeading, k_OriginPitch);
}

/// The point a start made by a land's script looks at: the place on the land, raised by the place's own altitude (the
/// height given after a second comma, none in the shipped scripts)
[[nodiscard]] inline glm::vec3 FocusOf(glm::vec3 onTheLand, const map_coords::MapCoords& place)
{
	return {onTheLand.x, onTheLand.y + place.altitude, onTheLand.z};
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
