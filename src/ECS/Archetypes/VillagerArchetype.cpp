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

#include "3D/ObjectMatrix.h"
#include "ECS/DetailMeshes.h"
#include "ECS/VillagerSpeed.h"
#include "ECS/MapCells.h"
#include "ECS/SeaCells.h"
#include "ECS/Villager/VillagerCore.h"
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

entt::entity VillagerArchetype::Create([[maybe_unused]] const glm::vec3& abodePosition, const glm::vec3& position,
                                       VillagerInfo type, uint32_t age, [[maybe_unused]] bool joinTown)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);

	const auto& info = Locator::infoConstants::value().villager.at(static_cast<size_t>(type));

	// Villager::Create 0x74FBE0: the first draw (GameRand(10) <= 1 tries a SpecialVillager; TODO(V14))
	ecs::villager::RollSpecialVillager();

	// (inferido) the first angle of Villager::Create 0x74FBE0 -> ctor 0x74F950 is not read: kept at glm's +pi
	registry.Assign<Transform>(entity, position, lh_matrix::AngleY(-glm::radians(180.0f)), glm::vec3(1.0));
	registry.Assign<Mobile>(entity);
	// Living::Living 0x5EBEC0: SetLife(info.life); the rest of Villager comes from the constructor below
	auto& villager = registry.Assign<Villager>(entity);
	villager.life = info.life;
	// Villager::IsWoman 0x752620 reads GVillagerInfo +0x1F8 (sex): the component mirrors it
	villager.sex = info.sex == SexType::Female ? Villager::Sex::FEMALE : Villager::Sex::MALE;
	villager.tribe = info.tribeType;
	villager.number = info.villagerNumber;
	villager.task = Villager::Task::IDLE;
	villager.lifeStage = Villager::LifeStage::Adult;
	// Object::Object leaves the states (+0x8C) at 0 INVALID; the constructor sets the counter and the state
	registry.Assign<LivingAction>(entity, VillagerStates::InvalidState, static_cast<uint16_t>(0));
	// WallHug::speed is the distance moved per game turn (the u16 at +0x5A in MapCoords, GetSpeedInMetres 0x60C070), and
	// the speed groups are in m/s: a turn is 0.1 s
	registry.Assign<WallHug>(entity, glm::vec2(), glm::vec2(), 0.0f, GetSpeedStateSpeed(info.speedGroup.speedDefault) * 0.1f);

	// Villager::Villager 0x74F950 (ECS/Villager/VillagerCore.cpp): SetToZero, SetAge (its InitialiseScale +
	// SetScaleForAge draw here), food, lastCheckTurn, the state counter and the water rule (MapCoords::IsWater 0x6035B0:
	// 16 DROWNING, else 85 CREATED)
	const uint32_t turn = ecs::villager::CurrentTurn(); // g_game +0x205A40
	ecs::villager::Construct(entity, info, age, turn, ecs::sea_cells::IsWater(position) /* MapCoords::IsWater 0x6035B0 */, [&](uint32_t setAge) {
		registry.Get<Transform>(entity).scale = glm::vec3(ecs::VillagerScaleForAge(info, setAge));
		age = setAge;
	});
	const bool child = ecs::villager::IsChild(entity);

	// children have their own meshes (childMeshHigh..Low); LOD 1 like the original, or the high ones (mod)
	const auto resourceId = resources::HashIdentifier(ecs::detail_meshes::Villager(info, child));
	registry.Assign<Mesh>(entity, resourceId, static_cast<int8_t>(0), static_cast<int8_t>(0));

	// Villager::Create 0x74FBE0 houses no one: the map script's handler (CREATE_VILLAGER_POS 0x715AA8..0x715AE6,
	// FeatureScriptCommands.cpp) finds the town and calls Town::AddVillagerToTown 0x73A090, which picks the abode
	// (FindAbodeWithSpaceInTown 0x73B370, ecs::town_villagers). The constructor left town and abode at 0 (SetToZero)

	if (std::getenv("OPENBLACK_OBJECT_INDEX_TRACE") != nullptr)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Object index: villager {} at ({:.2f}, {:.2f}) type {} age {} meshes {} {}",
		                   ecs::object_index::Of(entity), position.x, position.z, static_cast<int>(type), age,
		                   static_cast<int>(info.highDetail), static_cast<int>(info.childMeshHigh));
	}
	// (aproximado) the original's first speed comes with the first SetTopState (CREATED -> 163); openblack sets the
	// CREATED one now, as before (CREATED does not walk, so it is not seen)
	ecs::SetVillagerStateSpeed(entity);
	// Villager::Create 0x74FBE0 -> CallVirtualFunctionsForCreation (MobileObject 0x607150+0xA9): InsertMapObject
	// (vt +0x544, Object 0x636740), the head of its cell's mobile list. (inferido) after the town and the house
	ecs::map_cells::InsertMapObject(entity);

	return entity;
}
