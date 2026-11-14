/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

namespace openblack
{
class EventManager;
}

namespace openblack::ecs::systems
{
class PathfindingSystemInterface
{
public:
	virtual ~PathfindingSystemInterface() = default;

	/// This turn's step of one villager's wall-hugging walk. The state functions that walk call it
	/// (living_turn::MoveToStep, ECS/LivingTurn.h)
	virtual void Step(entt::entity entity) = 0;
	/// The straight line's look ahead of one villager, from where it stands along the step it has: the nearest circle
	/// of its map cell's things ahead and the turns to it, or none. The walk's setups call it
	virtual void SweepAhead(entt::entity entity) = 0;
	/// A thing's collide data was let go of (ecs::events::CollideDataReleased): every walker going round a circle of
	/// one of the overlapping things may leave it at its next chance, then every walker holding one of the thing's
	/// circles lets go of it at once, as its move state says (wall_hug::OnCircleGone)
	virtual void OnCollideDataReleased(entt::entity thing, std::span<const entt::entity> overlapping) = 0;
};

/// Hands the map's CollideDataReleased to the Locator's pathfinding system, when there is one
void AddPathfindingEventHandlers(EventManager& manager);
} // namespace openblack::ecs::systems
