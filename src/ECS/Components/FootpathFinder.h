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

#include <memory>

#include <entt/entity/entity.hpp>

#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RoutePlan.h"

namespace openblack::ecs::components
{

/// A footpath being planned between two things over several game turns (a worship site's, from each town's storage
/// pit). Its footpath is made at once with the two ends; the planner then runs a few steps every turn, and when it has a
/// route the footpath gets its nodes and goes to both things' links (ecs::footpaths::ProcessFootpathFindersTurn)
struct FootpathFinder
{
	entt::entity from {entt::null};
	entt::entity to {entt::null};
	/// The footpath being planned: its head is from's arrive point, its tail to's
	entt::entity footpath {entt::null};
	/// The finders run newest first
	uint32_t creationStamp {0};
	/// The planner's own obstacle grid and plan, kept from turn to turn
	std::unique_ptr<route_planner::ObstacleGrid> holder;
	std::unique_ptr<route_planner::RoutePlan> plan;
};

} // namespace openblack::ecs::components
