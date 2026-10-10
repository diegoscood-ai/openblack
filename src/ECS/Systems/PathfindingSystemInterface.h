/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_set>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "ECS/Map.h"

namespace openblack::ecs::systems
{
class PathfindingSystemInterface
{
public:
	virtual ~PathfindingSystemInterface() = default;

	/// This turn's step of one villager's wall-hugging walk. The state functions that walk call it
	/// (living_turn::MoveToStep, ECS/LivingTurn.h)
	virtual void Step(entt::entity entity) = 0;

	/// Files every fixed thing afresh in the cells whose middles its bounding circle (a unit wider) takes in: the
	/// walkers' obstacles. The map's Sync calls it, before the cell lists
	virtual void FileObstacles() = 0;
	/// The walkers' obstacles in a cell, by their bounding circles, as last filed
	[[nodiscard]] virtual const std::unordered_set<entt::entity>& ObstaclesIn(const MapInterface::CellId& cell) const = 0;
};
} // namespace openblack::ecs::systems
