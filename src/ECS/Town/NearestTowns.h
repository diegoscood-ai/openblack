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

#include <optional>
#include <span>

#include <entt/entity/entity.hpp>

#include "Enums.h"

/// Each town's nearest town (components::Town::nearestTown): worked out again for every town whenever a town joins or
/// leaves a player's list of towns. docs/wiki/beings/towns.md#the-nearest-town
namespace openblack::ecs::nearest_towns
{

/// A town as the search sees it: its map cell (the high words of its position, read signed) and its tribe (none when
/// the entity has no tribe)
struct Place
{
	entt::entity town {entt::null};
	int32_t cellX {0};
	int32_t cellZ {0};
	std::optional<Tribe> tribe;
};

/// The search starts with this whole-cell distance as its best: a town further away is never found
constexpr uint32_t k_FarthestCells = 10000000u;

/// The whole-cell distance between two cells: the larger difference plus half the smaller, rounded down
[[nodiscard]] constexpr uint32_t CellDistance(int32_t dx, int32_t dz)
{
	const int32_t x = dx < 0 ? -dx : dx;
	const int32_t z = dz < 0 ? -dz : dz;
	return static_cast<uint32_t>(x < z ? (x >> 1) + z : (z >> 1) + x);
}

/// The nearest town to `from` in the walk (every player's list of towns in order, the neutral player's last): strictly
/// nearer than the best so far, so of two equally near the first walked is kept; never `from` itself nor a town of no
/// tribe. Null when there is none
[[nodiscard]] entt::entity NearestOf(std::span<const Place> walk, const Place& from);

/// Every town's nearest town worked out again, over the towns in their players' lists; a town that finds none keeps the
/// one it had. `leaving` is a town that has just left its player's list: it is not walked and keeps its own
void UpdateAll(entt::entity leaving = entt::null);

} // namespace openblack::ecs::nearest_towns
