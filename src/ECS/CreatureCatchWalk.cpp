/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureCatchWalk.h"

#include "Creature/CreatureCatch.h"
#include "ECS/PhysicsHooksWorld.h"

using namespace openblack;
using namespace openblack::ecs;

void creature_catch_walk::Offer(physics_hooks::World& world, entt::entity object, const Flight& flight)
{
	for (const auto creature : world.Creatures())
	{
		const auto eligibility = world.CatchEligibility(creature);
		// The hand's hold is asked again on its own, as the game does
		if (!creature_catch::MayTry(eligibility) || eligibility.heldByHand)
		{
			continue;
		}
		// Lighter than the most it can catch, and the thing's own state lets it be caught
		if (!(world.ThingWeight(object) < creature_catch::MostWeight(world.CreatureWeight(creature))) ||
		    !world.MayBeCaught(object))
		{
			continue;
		}
		// Not its own throw; that of a player neither its own nor allied to its own only now and then
		if (flight.thrower == creature)
		{
			continue;
		}
		if (flight.player.has_value() && *flight.player != world.OwnerOf(creature) &&
		    !world.IsAllied(*flight.player, world.OwnerOf(creature)) &&
		    !creature_catch::GoesForOtherPlayersThrow(world.GameRand(creature_catch::k_OtherPlayersDraw)))
		{
			continue;
		}
		auto approach = world.CatchApproach(creature);
		if (!approach.has_value())
		{
			continue;
		}
		approach->thing = flight.centre;
		approach->velocity = flight.velocity;
		if (!creature_catch::Reaches(*approach))
		{
			continue;
		}
		world.SetAboutCatching(creature, object);
	}
}
