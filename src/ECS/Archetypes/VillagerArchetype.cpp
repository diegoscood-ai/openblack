/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArchetype.h"

#include <algorithm>
#include <cstdlib>

#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "Common/RandomNumberManager.h"
#include "ECS/DetailMeshes.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/ObjectCreationIndex.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

entt::entity VillagerArchetype::Create(const glm::vec3& abodePosition, const glm::vec3& position,
                                       VillagerInfo type, uint32_t age, bool joinTown)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(type));

	registry.Assign<Transform>(entity, position, glm::eulerAngleY(glm::radians(180.0f)), glm::vec3(1.0));
	registry.Assign<Mobile>(entity);
	const uint32_t health = 100;
	const uint32_t hunger = 100;

	// Villager::SetAge (0x7528C0): a child below grownUpAge (13), else an adult of at least 18
	const auto lifeStage = age < info.grownUpAge ? Villager::LifeStage::Child : Villager::LifeStage::Adult;
	if (lifeStage == Villager::LifeStage::Adult)
	{
		age = std::max<uint32_t>(age, 18);
	}
	// its size (InitialiseScale + SetScaleForAge)
	registry.Get<Transform>(entity).scale = glm::vec3(ecs::VillagerScaleForAge(info, age));
	const auto sex = info.villagerNumber == VillagerNumber::Housewife ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	const auto task = Villager::Task::IDLE;

	// TODO(bwrsandman): Might be better to make a FindClosestAbode
	const entt::entity town = joinTown ? Locator::townSystem::value().FindClosestTown(abodePosition) : entt::null;
	entt::entity abode = entt::null;
	if (town != entt::null)
	{
		// the villager lives in the house at the script's abode position (the nearest one), else any with space
		float nearest = 1.0f;
		registry.Each<const Abode, const Transform>([&](entt::entity candidate, const Abode& /*unused*/, const Transform& transform) {
			const glm::vec2 d(transform.position.x - abodePosition.x, transform.position.z - abodePosition.z);
			const float distance2 = glm::dot(d, d);
			if (distance2 < nearest)
			{
				nearest = distance2;
				abode = candidate;
			}
		});
		if (abode == entt::null)
		{
			abode = Locator::townSystem::value().FindAbodeWithSpace(town);
		}
		if (abode != entt::null)
		{
			registry.Get<Abode>(abode).inhabitants.insert(entity);
		}
	}

	registry.Assign<Villager>(entity, health, static_cast<uint32_t>(age), hunger, lifeStage, sex, info.tribeType,
	                          info.villagerNumber, task, town, abode);
	// WallHug::speed is the distance moved per game turn (the u16 at +0x5A in MapCoords, GetSpeedInMetres 0x60C070), and
	// the speed groups are in m/s: a turn is 0.1 s
	registry.Assign<WallHug>(entity, glm::vec2(), glm::vec2(), 0.0f, GetSpeedStateSpeed(info.speedGroup.speedDefault) * 0.1f);
	// children have their own meshes (childMeshHigh..Low); LOD 1 like the original, or the high ones (mod)
	const auto resourceId =
	    resources::HashIdentifier(ecs::detail_meshes::Villager(info, lifeStage == Villager::LifeStage::Child));
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	if (std::getenv("OPENBLACK_OBJECT_INDEX_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Object index: villager {} at ({:.2f}, {:.2f}) type {} age {} meshes {} {}",
		                   ecs::object_index::Of(entity), position.x, position.z, static_cast<int>(type), age,
		                   static_cast<int>(info.highDetail), static_cast<int>(info.childMeshHigh));
	}
	auto turnsSinceStateChange = Locator::rng::value().NextValue<uint16_t>(1, 500);
	registry.Assign<LivingAction>(entity, VillagerStates::Created, turnsSinceStateChange);
	ecs::SetVillagerStateSpeed(entity);

	return entity;
}
