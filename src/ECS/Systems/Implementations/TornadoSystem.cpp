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

#include "ECS/Physics/ParticleCarriedObjects.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "Particles/Rules/Storm.h"

using namespace openblack;
using namespace openblack::ecs::systems;

bool TornadoSystem::Carry(entt::entity object)
{
	// not available (a dying villager too): nothing carried
	if (!physics::particle_carried_objects::IsAvailable(object))
	{
		return false;
	}
	// already carried by a particle: kept, with no second take
	if (physics::particle_carried_objects::IsCarried(object))
	{
		return true;
	}
	// in flight: nothing; else the take, which may refuse it too (in physics with no body, a living one flying, out of
	// the map cells, marked as carried)
	return !physics::PhysicsObjects::IsFlying(object) && physics::particle_carried_objects::Take(object);
}

void TornadoSystem::Update()
{
	psys::storm::UpdateCarriedObjects();
}

size_t TornadoSystem::CarriedCount() const
{
	return psys::storm::CarriedObjectCount();
}
