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

namespace openblack::ecs::components
{

/// Simulation-only things of the map script. None of them is drawn when the land loads, and openblack doesn't simulate
/// them yet: they are kept as data. Each is an entity of its own, without a Transform.

/// CREATE_ARENA (0x7175FA -> fn_00424820 -> GArena ctor 0x4246F0, list g_game+0x205C7C): a creature fight arena. Its
/// GLightSheet (grey 180 light on the land) is only drawn while a fight is on (GArena::Draw 0x4249E0, +0x34).
struct Arena
{
	glm::vec3 position;
	float radius; ///< +0x30 (GetRadius)
};

// CREATE_WEATHER_CLIMATE(_RAIN/_TEMP/_WIND) make the GClimates of ECS/Weather/Climate (the one copy: id, info,
// radii, rain +0x34.., temperatures +0x44/+0x48, wind +0x4C..; the world's climate g_game+0x250534).

/// CREATE_DRINK_WAYPOINT (0x716616 -> 0x770BC0, WayPoint.cpp, list g_game+0x205C74): an invisible point where the
/// creature goes to drink.
struct DrinkWaypoint
{
	glm::vec3 position;
};

} // namespace openblack::ecs::components
