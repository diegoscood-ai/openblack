/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <optional>
#include <span>

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

// The apple barrel with its worm: a little ornament the land draws beside the local player's storage pit, the pit of
// their towns nearest their temple. The barrel stands on the land at the pit's edge, on the side the pit faces, never
// turned and at its own size; the worm sits on the barrel's point, wriggling on its clip by the game's clock. Neither is
// a thing of the world: the hand, the creature and the villagers never meet them. Pure functions, tested on their own
// (raffclar's rules, on the map's own coordinates). Wiki: docs/wiki/rendering/frame.md#the-apple-barrel

namespace openblack::apple_barrel
{

/// No storage pit this many metres or further from the temple carries the barrel
constexpr float k_FarthestStore = 99990.0f;
/// With no temple the pits are measured from this place on the map instead: 256 metres along x and along z, one map
/// unit short (2^24 / 10 truncated)
constexpr map_coords::MapCoords k_NoTemplePlace {.x = 1677721, .z = 1677721, .altitude = 0.0f};

/// Where the pits are measured from: the player's temple, or the fixed place on the map when they have none
[[nodiscard]] map_coords::MapCoords MeasuredFrom(const std::optional<map_coords::MapCoords>& temple);

/// Which of the pits carries the barrel, from their distances in metres to that place, in the order of the player's
/// towns: the first one nearer than any before it and than the farthest allowed; none when there is none
[[nodiscard]] std::optional<size_t> NearestStore(std::span<const float> distances);

/// Where the barrel stands on the map: out from the pit's place by its radius on the ground, at the pit's angle about
/// the vertical, its altitude 0
[[nodiscard]] map_coords::MapCoords BarrelPlace(const map_coords::MapCoords& store, float yAngle, float radius);

/// Where the ground under the barrel is read: its place in metres taken back to the map, which may lose a unit
[[nodiscard]] map_coords::MapCoords GroundCoords(const map_coords::MapCoords& place);

/// Where the worm sits: the barrel's model's point for it, the barrel standing unturned at its own size
[[nodiscard]] glm::vec3 WormSpot(const glm::vec3& barrel, const glm::vec3& modelPoint);

/// The worm's place in its clip after so many milliseconds of the game's clock: a looping clip comes round again, any
/// other stops a millisecond short of its end
[[nodiscard]] uint32_t StepWorm(uint32_t place, uint32_t elapsed, uint32_t playTime, bool looping);

} // namespace openblack::apple_barrel
