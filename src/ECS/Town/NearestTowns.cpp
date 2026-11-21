/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "NearestTowns.h"

#include <vector>

#include "3D/MapCoords.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "Locator.h"

namespace openblack::ecs::nearest_towns
{

entt::entity NearestOf(std::span<const Place> walk, const Place& from)
{
	uint32_t best = k_FarthestCells;
	entt::entity nearest = entt::null;
	for (const auto& place : walk)
	{
		const auto distance = CellDistance(from.cellX - place.cellX, from.cellZ - place.cellZ);
		if (distance < best && place.town != from.town && place.tribe != Tribe::NONE)
		{
			best = distance;
			nearest = place.town;
		}
	}
	return nearest;
}

void UpdateAll(entt::entity leaving)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<Place> walk;
	map_cells::ForEachTown([&](entt::entity town) {
		if (town != leaving)
		{
			// only the town's cell is needed, so its place is read without the ground's height under it
			const auto* transform = registry.TryGet<const components::Transform>(town);
			const glm::vec3 at = transform != nullptr ? transform->position : glm::vec3 {0.0f};
			const auto* tribe = registry.TryGet<const Tribe>(town);
			walk.push_back({
			    .town = town,
			    .cellX = map_coords::SignedCellOf(map_coords::ToFixed(at.x)),
			    .cellZ = map_coords::SignedCellOf(map_coords::ToFixed(at.z)),
			    .tribe = tribe != nullptr ? std::optional {*tribe} : std::nullopt,
			});
		}
		return true;
	});
	// a town that finds none keeps the one it had
	for (const auto& place : walk)
	{
		if (const auto nearest = NearestOf(walk, place); nearest != entt::null)
		{
			registry.Get<components::Town>(place.town).nearestTown = nearest;
		}
	}
}

} // namespace openblack::ecs::nearest_towns
