/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/MapGridCells.h"
#include "ECS/Systems/PathfindingSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp, use interface instead."
#endif

namespace openblack::ecs::systems
{
class PathfindingSystem final: public PathfindingSystemInterface
{
public:
	void Step(entt::entity entity) override;
	void FileObstacles() override;
	[[nodiscard]] const std::unordered_set<entt::entity>& ObstaclesIn(const MapInterface::CellId& cell) const override;

private:
	/// The walkers' obstacles, one set per cell, x plus z times the cells a side. It lives as long as the level, and
	/// clearing a cell keeps its set's buckets, so the sets list their things in the same order every turn
	MapGridCells<MapInterface::k_GridSize.x * MapInterface::k_GridSize.y> _obstacles;
};
} // namespace openblack::ecs::systems
