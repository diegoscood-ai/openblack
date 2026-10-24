/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <chrono>
#include <optional>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack
{
class LandIslandInterface;
}

/// The player camera's view of a point it flies to: from which side, how far and how tilted it looks at it. Pure
/// maths over the land, tested with a fake one; the world camera makes the flight
namespace openblack::camera_flight
{

/// Distances below 50 are brought most of the way up to it, (50 - d) x 0.8 + d; then those above 100 a tenth of the way
/// down to it, (100 - d) x 0.1 + d. Not a number stays one
[[nodiscard]] float ShapeDistance(float distance);

/// The best of 32 headings round a point, heading + i x pi / 16, to look at it from `distance` away: each scores how
/// far the point stands above the land at 3/8 .. 7/8 of the distance that way, plus 50 x cos(i x pi / 16), and the
/// first highest wins. `pitch` becomes a fifth of itself, plus a tenth of the land's own tilt there and 3 pi / 25, kept
/// within pi / 8 and pi / 3 (not a number goes to pi / 8). The land is read at the map's fixed point of each x and z
[[nodiscard]] float FindBestAngle(const LandIslandInterface& land, float heading, float distance, glm::vec3 point,
                                  float& pitch);

/// Where the camera goes to look at `focus` as if from `from`
struct View
{
	glm::vec3 origin;
	glm::vec3 focus;
};
/// The view of a point: looking at `focus` from `from`'s side, at their distance shaped by ShapeDistance, turned to
/// the best heading and tilted as FindBestAngle gives them
[[nodiscard]] View ViewOfPoint(const LandIslandInterface& land, glm::vec3 from, glm::vec3 focus);

/// The point a watched fight's flight looks at: the arena's rim on the +x side, half its radius above its centre
[[nodiscard]] glm::vec3 ArenaLookPoint(glm::vec3 centre, float radius);

/// The point a double click flies to. With an object under the hand: the object's place on the map, at the land's
/// height there read at the map's fixed point; else the land under the mouse, and none without it
[[nodiscard]] std::optional<glm::vec3> DoubleClickPoint(const LandIslandInterface& land,
                                                        std::optional<map_coords::MapCoords> object,
                                                        std::optional<glm::vec3> landUnderMouse);

/// The camera's easing time a flight starts it at, from the time since the flight began that the flight starts with:
/// the game keeps the two as one, so a short flight with no woosh eases from halfway down the easing's ramp
[[nodiscard]] std::chrono::microseconds EasingTimeAtStart(float flightSeconds);

} // namespace openblack::camera_flight
