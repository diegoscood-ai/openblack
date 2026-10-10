/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#include "MapProduction.h"

#include <glm/vec3.hpp>

#include "ECS/Systems/PathfindingSystemInterface.h"
#include "Locator.h"
#include "MapCells.h"

using namespace openblack::ecs;

std::span<const entt::entity> MapProduction::GetFixedInGridCell(const CellId& cellId) const
{
	_fixedView.clear();
	map_cells::ForEachFixed(glm::ivec2(cellId), [this](entt::entity entity) {
		_fixedView.push_back(entity);
		return true;
	});
	return _fixedView;
}

std::span<const entt::entity> MapProduction::GetFixedInGridCell(const glm::vec3& pos) const
{
	return GetFixedInGridCell(GetGridCell(pos));
}

std::span<const entt::entity> MapProduction::GetMobileInGridCell(const CellId& cellId) const
{
	_mobileView = map_cells::MobileInCell(glm::ivec2(cellId));
	return _mobileView;
}

std::span<const entt::entity> MapProduction::GetMobileInGridCell(const glm::vec3& pos) const
{
	return GetMobileInGridCell(GetGridCell(pos));
}

std::vector<entt::entity> MapProduction::GetAllInCell(glm::ivec2 cell) const
{
	return map_cells::ObjectsInCell(cell);
}

void MapProduction::Sync()
{
	// the walkers' obstacles first, then the cell lists, as one rebuild of the map
	if (Locator::pathfindingSystem::has_value())
	{
		Locator::pathfindingSystem::value().FileObstacles();
	}
	// the ordered cell lists take in what the owners without hooks did since the last sync
	map_cells::Sync();
}

void MapProduction::Refile(entt::entity entity)
{
	// out of its cells and in again at the front, or in for the first time
	if (map_cells::IsObjectInMap(entity))
	{
		map_cells::OnAnglesOrScaleChanged(entity);
	}
	else
	{
		map_cells::InsertMapObject(entity);
	}
}
