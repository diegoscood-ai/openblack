/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BallKick.h"

#include "ECS/Components/Ball.h"
#include "ECS/FootballRules.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGround.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"

namespace openblack::ecs::ball_kick
{

bool KickBallAtDestination(entt::entity ball, const map_coords::MapCoords& destination, float speed, bool scoring)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(ball) || !registry.AllOf<components::Ball>(ball) || !Locator::dynamicsSystem::has_value())
	{
		return false;
	}
	auto& dynamics = Locator::dynamicsSystem::value();
	const auto from = map_coords::ToWorld(object::MapCoordsOf(ball));
	const auto to = map_coords::ToWorld(destination);
	const auto velocity = football_rules::KickVelocity(from, to, speed);
	// a ball in flight leaves the physics first. (inferred) with its end of physics, which puts it back on the map
	if (dynamics.IsFlying(ball))
	{
		dynamics.RemoveObjectWithEndPhysics(ball);
	}
	if (!map_cells::IsObjectInMap(ball))
	{
		return false;
	}
	auto* entry = dynamics.AddObject(ball, velocity, systems::DynamicsSystemInterface::BodySpin {});
	if (entry == nullptr)
	{
		return false;
	}
	entry->flags |= PhysicsEntry::k_PushedByLiving;
	// onto the land along its slope, lowered as well as raised, the turn-start pose following
	entry->body.SettleOnLand(GameGround(), false, true);
	dynamics.SyncTurnStart(*entry);
	auto& b = registry.Get<components::Ball>(ball);
	b.kick = scoring ? 1u : 0u;
	b.destination = to;
	return true;
}

} // namespace openblack::ecs::ball_kick
