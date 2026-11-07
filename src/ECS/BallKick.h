/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

/// A football kicked at a point (docs/wiki/world/buildings.md#the-football-pitch).
namespace openblack::ecs::ball_kick
{

/// The ball is kicked to land on `destination` at `speed` (football_rules::KickVelocity): a ball already in flight
/// leaves the physics first; one off the map is not kicked. In flight, it is marked as pushed by a living, put onto
/// the land along its slope, and keeps the kick's mark (`scoring`: only a scoring kick lets it score) and its
/// destination. Whether it flies
bool KickBallAtDestination(entt::entity ball, const map_coords::MapCoords& destination, float speed, bool scoring);

} // namespace openblack::ecs::ball_kick
