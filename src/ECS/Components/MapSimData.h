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

/// CREATE_WEATHER_CLIMATE (0x7171F5 -> fn_00771300 -> GClimate ctor 0x771170, list g_game+0x205CF4). Id 0 is the
/// world's climate (GClimate(0) 0x771020, which ignores the position and radii; the _RAIN/_TEMP/_WIND commands with id
/// 0 use g_game+0x250534, made on demand). The ctor starts rain and temperature from the info's season ranges; not
/// ported, the script sets them next.
struct Climate
{
	int32_t id;       ///< +0x28, what the _RAIN/_TEMP/_WIND commands look it up by (fn_007731B0, newest first)
	int32_t info {0}; ///< +0x2C: GClimateInfo index (ClimateInfo)
	glm::vec3 position {0.0f};
	float innerRadius {0.0f}; ///< +0x20: the smaller of the script's two radii
	float outerRadius {0.0f}; ///< +0x24: the larger

	/// CREATE_WEATHER_CLIMATE_RAIN (0x717250 -> 0x773200): +0x34.. = {F1, N2, N3, (uint8_t)N4}
	float rain {0.0f};
	int32_t rainN2 {0};
	int32_t rainN3 {0};
	uint8_t rainN4 {0};
	/// CREATE_WEATHER_CLIMATE_TEMP (0x7172A2 -> 0x773290): +0x44 = F1, +0x48 = F2
	float temperature1 {0.0f};
	float temperature2 {0.0f};
	/// CREATE_WEATHER_CLIMATE_WIND (0x7172E0 -> 0x7732D0): +0x4C.. = {F1, F2, F3}
	float wind1 {0.0f};
	float wind2 {0.0f};
	float wind3 {0.0f};
};

/// CREATE_DRINK_WAYPOINT (0x716616 -> 0x770BC0, WayPoint.cpp, list g_game+0x205C74): an invisible point where the
/// creature goes to drink.
struct DrinkWaypoint
{
	glm::vec3 position;
};

} // namespace openblack::ecs::components
