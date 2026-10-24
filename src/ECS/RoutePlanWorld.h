/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "RoutePlanner/Point2D.h"

// The route planner's world side: what
// the ObstacleGrid sees of the land: the central land_avoid mask (3D/LandAvoid.h, built at every land load) and the
// holder's square callback is CheckSquareFunction (installed at game creation). The
// objects add their circles through AddToRoutePlan when CreatureMustAvoid says so: with no creature for the footpaths,
// with the plan's creature for a creature's follower. The citadel heart's worship site circles in a creature's plan
// are (pending, the creature port).

namespace openblack::route_planner
{
class ObstacleGrid;
} // namespace openblack::route_planner

namespace openblack::ecs::route_plan_world
{
/// The push-out callback: instead of AddObject, each circle the object would add
using CircleFn = void (*)(void* context, entt::entity object, const route_planner::Point2D& centre, float radius,
                          int32_t notify);

/// ObstacleGrid::InstallCallbacks(CheckSquareFunction, nullptr), at every land load (after land_avoid::Validate)
void OnLandLoaded();

/// the cell's 3 x 3 LandAvoid circles, then its objects' (fixed list, then
/// mobile list)
void CheckSquareFunction(int32_t cellX, int32_t cellZ, route_planner::ObstacleGrid& holder);
/// whether the creature must avoid the object, with no creature
[[nodiscard]] bool CreatureMustAvoid(entt::entity object);
/// How a kind of thing decides whether a given creature must avoid it, by the original's rule for its class
enum class AvoidRule : uint8_t
{
	Never,                 ///< livings, fish farms, the football pitch, prayer sites, street lanterns, town flags
	Always,                ///< animated statics and chess pieces
	WhileBurning,          ///< a big forest, a fragment
	BurningUnlessScripted, ///< a dead or felled tree, a field, a food pile: burning, and the creature not a script's
	TallOrBurning,         ///< any other thing: at least a tenth of the creature's height, or burning
	BuildingTallOrBurning, ///< a building: begun and at least a tenth of the creature's height, or burning
	TreeTallOrBurning,     ///< a tree: at least one and a half times the creature's height, or burning
};
/// What the rules read of the thing and the creature
struct AvoidFacts
{
	float height {0.0f};         ///< the thing's height
	float creatureHeight {0.0f}; ///< the creature's height
	bool onFire {false};
	bool creatureScripted {false}; ///< a script controls the creature
	bool begun {true};             ///< a building: more than a hundredth built, or with scaffolds on its site
};
/// Whether a creature must avoid a thing with these facts by its class's rule
[[nodiscard]] bool MustAvoidByRule(AvoidRule rule, const AvoidFacts& facts);
/// whether this creature must avoid the object: another player's map shield; a way out of the land the creature may
/// not be taken through; else the rule of the object's class (MustAvoidByRule). (pending) another creature is avoided
/// by the original while its route follows a path, which openblack does not tell yet: it never is
[[nodiscard]] bool CreatureMustAvoid(entt::entity object, entt::entity creature);
/// the object's circles; in a creature's plan (creature not entt::null) they keep the creature's share of its own radius
/// and the margin off, and 5.0 more round a thing with a fire
void AddToRoutePlan(entt::entity object, route_planner::ObstacleGrid& holder, int32_t notify,
                    entt::entity creature = entt::null, CircleFn callback = nullptr, void* context = nullptr);

/// x, z x 10 / 65536, each rounded once to single precision (the altitude read and dropped)
[[nodiscard]] route_planner::Point2D ToPoint2D(const map_coords::MapCoords& coords);
/// truncated x x 6553.6, z x 6553.6, altitude 0
[[nodiscard]] map_coords::MapCoords ToMapCoords(const route_planner::Point2D& point);
/// The circles' id: the object
[[nodiscard]] int32_t IdOf(entt::entity object);
} // namespace openblack::ecs::route_plan_world
