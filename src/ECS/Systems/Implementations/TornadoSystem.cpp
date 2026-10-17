/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TornadoSystem.h"

#include <cstdio>
#include <cstdlib>

#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"
#include "Particles/Rules/Storm.h"

using namespace openblack;
using namespace openblack::ecs::systems;

bool TornadoSystem::Carry(entt::entity object)
{
	// not available (a dying villager too): nothing carried
	if (!ecs::physics::particle_carried_objects::IsAvailable(object))
	{
		return false;
	}
	// already carried by a particle: kept, with no second take
	if (ecs::physics::particle_carried_objects::IsCarried(object))
	{
		return true;
	}
	// in flight: nothing; else the take, which may refuse it too (in physics with no body, a living one flying, out of
	// the map cells, marked as carried)
	return !Locator::dynamicsSystem::value().IsFlying(object) && ecs::physics::particle_carried_objects::Take(object);
}

void TornadoSystem::Update()
{
	psys::storm::UpdateCarriedObjects();
}

void TornadoSystem::CatchCreature(entt::entity creature)
{
	if (!Locator::creatureFightSystem::has_value())
	{
		std::fputs("TornadoSystem::CatchCreature: the creature fight system is missing (Locator::creatureFightSystem)\n",
		           stderr);
		std::abort();
	}
	auto& fights = Locator::creatureFightSystem::value();
	// A creature fainted already isn't caught again until it has come round; else it faints where it is, lying
	// helpless, the leash letting go of it, and comes round as from a knock-out
	if (!Locator::entitiesRegistry::value().Valid(creature) || fights.IsKnockedOut(creature))
	{
		return;
	}
	fights.ForceFaint(creature);
}

size_t TornadoSystem::CarriedCount() const
{
	return psys::storm::CarriedObjectCount();
}
