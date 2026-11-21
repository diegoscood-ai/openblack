/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameDynamicsWorld.h"

#include "3D/LandIslandInterface.h"
#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "Camera/Camera.h"
#include "ECS/Registry.h"
#include "ECS/SnowDust.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

Registry& GameDynamicsWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

const Registry& GameDynamicsWorld::Entities() const
{
	return Locator::entitiesRegistry::value();
}

const LandIslandInterface* GameDynamicsWorld::Land() const
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}

const InfoConstants* GameDynamicsWorld::Info() const
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

std::optional<glm::vec3> GameDynamicsWorld::CameraOrigin() const
{
	return Locator::camera::has_value() ? std::optional(Locator::camera::value().GetOrigin()) : std::nullopt;
}

int32_t GameDynamicsWorld::SnowAt(glm::vec3 point) const
{
	return ecs::snow_dust::SnowAt(point);
}

uint32_t GameDynamicsWorld::LandColour() const
{
	return land_light::CurrentTable().GetLandColour();
}

filesystem::FileSystemInterface& GameDynamicsWorld::FileSystem()
{
	return Locator::filesystem::value();
}

resources::ResourcesInterface& GameDynamicsWorld::Resources()
{
	return Locator::resources::value();
}

BuildingDamageSystemInterface& GameDynamicsWorld::BuildingDamage()
{
	return Locator::buildingDamageSystem::value();
}
