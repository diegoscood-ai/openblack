/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameBuildingDamageWorld.h"

#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

Registry& GameBuildingDamageWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

const Registry& GameBuildingDamageWorld::Entities() const
{
	return Locator::entitiesRegistry::value();
}

resources::ResourcesInterface& GameBuildingDamageWorld::Resources()
{
	return Locator::resources::value();
}

DynamicsSystemInterface& GameBuildingDamageWorld::Dynamics()
{
	return Locator::dynamicsSystem::value();
}
