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

#include <glm/vec3.hpp>

#include "3D/MapCoords.h"

namespace openblack::ecs::components
{

/// Simulation-only things of the map script. None of them is drawn when the land loads, and openblack doesn't simulate
/// them yet: they are kept as data. Each is an entity of its own, without a Transform.

/// CREATE_ARENA: a creature fight arena. Its ring of white light on the land is only drawn while a fight is on in it,
/// which the script's arenas do not have yet.
struct Arena
{
	glm::vec3 position;
	float radius; ///< its radius
	/// Its place as the script gave it: in map units, with the script's height above the land (0 when it gave none)
	map_coords::MapCoords at {};
	/// The order the arenas were made in, from 1: the newest has the largest
	uint32_t number {0};
};

// CREATE_WEATHER_CLIMATE(_RAIN/_TEMP/_WIND) make the climates of ECS/Weather/Climate (the one copy: id, info, radii,
// rain, temperatures, wind; and the world's climate).

/// CREATE_DRINK_WAYPOINT: an invisible point where the creature goes to drink.
struct DrinkWaypoint
{
	glm::vec3 position;
	/// The order in which the land made the water points, from 0
	uint32_t number {0};
};

} // namespace openblack::ecs::components
