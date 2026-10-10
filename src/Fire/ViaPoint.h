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

#include <span>

#include "3D/MapCoords.h"

/// How a villager finds its way round a fire: the point to make for to pass a circle, all in map units as the game
/// works it out. Pure rules, tested without the game.
namespace openblack::fire
{

/// The result of looking for a way round a circle
struct ViaPoint
{
	/// Which way round, as the angle turned; 0 when the way needs no detour or the detour is no good. Only its sign and
	/// whether it is 0 matter to the caller
	float angle {0.0f};
	map_coords::MapCoords point {};
	bool detour {false};
	/// The start, or the end once a detour was needed, lies inside the circle
	bool inside {false};
};

/// The point to make for to pass a circle of a radius, in metres, by a margin on the way from one point to another, on
/// the side a preference gives when it has one (the angle a last detour turned), else on the side the way leans to.
/// There is no detour when the way clears the circle, when the start is inside it or within the margin of it, or when
/// the way has no length. The angle is 0 when the detour would be longer than the way itself.
[[nodiscard]] ViaPoint GetViaPoint(const map_coords::MapCoords& from, const map_coords::MapCoords& to,
                                   const map_coords::MapCoords& centre, float radius, float margin, float side);

/// A fire to keep clear of: where it is and how far off, in metres
struct Circle
{
	map_coords::MapCoords centre {};
	float radius {0.0f};
};

/// After this many detours the villager gives up looking for a way round this turn
constexpr int k_MostDetours = 1000;

/// What a villager going round a fire does
enum class WayRoundOutcome : uint8_t
{
	/// It walks to the point found
	Walk,
	/// Where it was going lies in a fire: it stops there and decides again
	Stop,
	/// No way round was found
	GiveUp,
};

struct WayRound
{
	WayRoundOutcome outcome {WayRoundOutcome::Walk};
	/// Where to walk, or where to stop
	map_coords::MapCoords target {};
};

/// The way from a villager to where it is going round the fire it reacts to, then round every fire of that fire's
/// group (in the group's order, the fire itself among them). Each detour makes the villager look at the whole group
/// again on the way to the new point, keeping to the side of the last detour, up to k_MostDetours times. The margin is
/// how far beyond a fire's radius it keeps, in metres.
[[nodiscard]] WayRound FindWayRound(const map_coords::MapCoords& from, const map_coords::MapCoords& destination,
                                    const Circle& reactedTo, std::span<const Circle> group, float margin);

} // namespace openblack::fire
