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
#include <glm/vec2.hpp>

// Where a walker (a villager or an animal) is going: every walk's goal is set through here, with its height above the
// land there.

namespace openblack::ecs::components
{
/// The height above the land of a walker's goal, while it is not 0: only a script's walk to a point off the land gives
/// one (the point's y less the land's height there)
struct WalkGoalAltitude
{
	float altitude {0.0f};
};
} // namespace openblack::ecs::components

namespace openblack::ecs::walk_goal
{

/// The walker's goal (its wall hug's or its brain's, given as `goal`) set to `to`, with no height above the land. (approximate)
/// the walks the game plans itself are given their goals on the land, where the original keeps the height above the land
/// of the point each is made from: for a point on the land, the land's own difference between that point and its place
/// on the map, under a millimetre
void Set(entt::entity walker, glm::vec2& goal, glm::vec2 to);
/// The height above the land of the goal just set (a script's walk)
void SetAltitude(entt::entity walker, float altitude);
/// The height above the land of the walker's goal: 0 unless a script's walk gave it one
[[nodiscard]] float AltitudeOf(entt::entity walker);

} // namespace openblack::ecs::walk_goal
