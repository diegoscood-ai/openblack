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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"

/// The hand's press with nothing under the cursor: the object it takes near the land behind the hand. Wiki:
/// docs/bw1-notes/hand-and-interface.md, "Object under the cursor".
namespace openblack::ecs::hand_near
{

/// What the search reads of an object it meets in a cell
struct Candidate
{
	/// Only x and z count: the distance is measured on the map
	glm::vec3 position;
	bool fragment {false};
};

/// The object nearest to `behind` (the land behind the hand) in the cells of the square +-5 m around it, or none.
/// The square is taken around `behind` as a map coordinate (x 10 / 65536; (c -+ 5) x 65536 / 10 truncated toward zero,
/// signed high words) and walked x outer, z inner; cells off the map are left out.
/// `walkCell(cell, visit)` calls visit(object) for each object of the cell, the fixed list then the mobile one, and stops
/// when visit returns false (it never does here). `lookup(object)` gives a std::optional<Candidate>: none for an object
/// the search skips (not available, or with no position). Every object but the fragments counts, at its 2D distance
/// from `behind` (GetDistanceInMetres); the nearest under 5 m (strict, so the first of a tie in the walk) is the one
/// taken, and only when it is not farther than the action's point `at` is from `behind` (<=)
template <typename WalkCell, typename Lookup>
[[nodiscard]] std::optional<entt::entity> NearestToLandBehindHand(glm::vec3 behind, glm::vec3 at, WalkCell&& walkCell,
                                                                  Lookup&& lookup)
{
	const float cx = map_coords::Quantise(behind.x);
	const float cz = map_coords::Quantise(behind.z);
	const int16_t minX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx - 5.0f));
	const int16_t maxX = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cx + 5.0f));
	const int16_t minZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz - 5.0f));
	const int16_t maxZ = map_coords::SignedCellOf(map_coords::ToFixedGUtils(cz + 5.0f));
	std::optional<entt::entity> best;
	float bestDistance = 5.0f;
	for (int32_t i = minX; i <= maxX; ++i)
	{
		for (int32_t j = minZ; j <= maxZ; ++j)
		{
			const glm::ivec2 cell(i, j);
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			walkCell(cell, [&](entt::entity object) {
				const std::optional<Candidate> candidate = lookup(object);
				if (!candidate || candidate->fragment)
				{
					return true;
				}
				const float distance = gutils::GetDistanceInMetres(behind, candidate->position);
				if (distance < bestDistance)
				{
					bestDistance = distance;
					best = object;
				}
				return true;
			});
		}
	}
	if (best && bestDistance <= gutils::GetDistanceInMetres(at, behind))
	{
		return best;
	}
	return std::nullopt;
}

} // namespace openblack::ecs::hand_near
