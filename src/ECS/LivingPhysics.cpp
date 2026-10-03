/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingPhysics.h"

#include <cmath>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/StoragePitStore.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerDrowning.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::physics;
using openblack::ecs::life::Kill;
using openblack::ecs::life::LifeOf;
using openblack::ecs::life::ReduceLife;

namespace
{

/// Living::ReactToPhysicsImpact's damage: Object::ApplyEffect with the crush preset g_EffectInfo[3] (crush 1.0) x the
/// object's defenceMultiplierCrush, then Object::ReduceLife. It dies at 0 (the dying states are not ported).
void HurtByImpact(entt::entity entity, float damage)
{
	const bool villager = Locator::entitiesRegistry::value().AllOf<Villager>(entity);
	const float life = ReduceLife(entity, damage);
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} hurt {:.3f}, life {:.2f}", villager ? "villager" : "animal", damage,
	                   life);
	if (life <= 0.0f)
	{
		if (Locator::entitiesRegistry::value().AllOf<Animal>(entity))
		{
			// Object::ApplyEffect -> Animal::DestroyedByEffect: SetDying (nothing while it flies, EndPhysics does it)
			ecs::animal_ai::DestroyedByEffect(entity);
			return;
		}
		Kill(entity, "impact");
	}
}

/// Living::ReactToPhysicsImpact 0x5ED3E0 (vt +0x7AC). Returns false when the entity went away.
bool LivingReactToPhysicsImpact(entt::entity entity, float g)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (g > 2.0f)
	{
		float multiplier = 1.0f;
		if (const auto* animal = registry.TryGet<const Animal>(entity); animal != nullptr && Locator::infoConstants::has_value())
		{
			multiplier = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type)).defenceMultiplierCrush;
		}
		HurtByImpact(entity, (g - 2.0f) * 0.03f * multiplier);
		return registry.Valid(entity);
	}
	return true;
}

/// Animal::ReactToPhysicsImpact (0x41BC10), then Living's.
bool AnimalReactToPhysicsImpact(entt::entity entity, [[maybe_unused]] PhysicsObject& po, const ImpactInfo& impact)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Animal::ReactToPhysicsImpact (0x41BC10): landing on an available food store it becomes food (info.foodValue)
	if (const auto* animal = registry.TryGet<const Animal>(entity);
	    animal != nullptr && impact.hitBy != entt::null && registry.Valid(impact.hitBy) && registry.AllOf<StoragePit>(impact.hitBy) &&
	    Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type));
		const auto food = static_cast<uint32_t>(info.foodValue);
		if (food > 0)
		{
			StoragePitStore::AddResource(impact.hitBy, ResourceType::Food, food);
			ecs::animal_ai::Remove(entity);
			return false;
		}
	}
	return LivingReactToPhysicsImpact(entity, impact.g);
}

/// Animal::EndPhysics (0x5F0D80), the class's part.
entt::entity AnimalEndPhysics(entt::entity entity, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Animal::EndPhysics (0x5F0D80): the landType from the body, back on the land (altitude 0) and out of the
	// physics; LANDED, or dying / dead. There is no drowning for animals (only a sunk corpse goes, HasSunk).
	// the landType is read from the turn-start matrix (po+0xD8), the heading from the current one
	const auto rotation = po.body.Rotation();
	auto& transform = registry.Get<Transform>(entity);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	ecs::animal_ai::EndPhysics(entity, rotation, po.turnStartRotation);
	registry.SetDirty();
	return entt::null;
}

/// Villager::EndPhysics, the class's part.
entt::entity VillagerEndPhysics(entt::entity entity, PhysicsObject& po)
{
	auto& registry = Locator::entitiesRegistry::value();
	// Villager/Animal::EndPhysics: stands up where it landed (the three landing poses are not done yet)
	auto& transform = registry.Get<Transform>(entity);
	const auto forward = transform.rotation[2];
	const float yaw = std::atan2(forward.x, forward.z);
	// yaw is glm's angle, the game's -yaw; (inferred) the angle EndPhysics gives is not read
	transform.rotation = lh_matrix::AngleY(-yaw);
	if (Locator::terrainSystem::has_value())
	{
		transform.position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(transform.position.x, transform.position.z));
	}
	// 0x5F0BAF: MapCoords::IsWater of Pos (the cell's water bit, the shallow shore too; not the body's inWater)
	if (ecs::sea_cells::IsWater(transform.position))
	{
		ecs::RememberLastPlayerToInteract(entity, po.byPlayer);
		ecs::VillagerEndPhysicsInWater(entity);
		return entt::null;
	}
	if (LifeOf(entity) <= 0.0f)
	{
		Kill(entity, "landed dead");
		return entt::null;
	}
	// Villager::EndPhysics: LANDED, its landing clip, then deciding what to do
	ecs::SetVillagerState(entity, VillagerStates::Landed);
	registry.SetDirty();
	return entity;
}

} // namespace

namespace openblack::ecs::living
{

void RegisterPhysicsHandlers()
{
	PhysicsObjects::ClassHandlers villager;
	// Living::InitialisePhysics: the villager flies (THROWN clips, ECS/VillagerAnimations)
	villager.initialisePhysics = [](entt::entity entity, PhysicsObject&, bool) {
		ecs::SetVillagerState(entity, VillagerStates::Flying);
	};
	// Living::ReactToPhysicsImpact 0x5ED3E0 (Villager shares it)
	villager.reactToImpact = [](entt::entity entity, PhysicsObject&, const ImpactInfo& impact) {
		return LivingReactToPhysicsImpact(entity, impact.g);
	};
	villager.endPhysics = VillagerEndPhysics;
	PhysicsObjects::SetClassHandlers(PhysicsClass::Villager, std::move(villager));

	PhysicsObjects::ClassHandlers animal;
	// Living::InitialisePhysicsFromHand: FLYING, the species' THROWN clip
	animal.initialisePhysics = [](entt::entity entity, PhysicsObject&, bool) { ecs::animal_ai::InitialisePhysics(entity); };
	animal.reactToImpact = AnimalReactToPhysicsImpact;
	animal.endPhysics = AnimalEndPhysics;
	PhysicsObjects::SetClassHandlers(PhysicsClass::Animal, std::move(animal));
}

void InterfaceSetInMagicHand(entt::entity entity)
{
	// Living::PlaceInHand: IN_HAND (its clip SCARED_STIFF, ECS/VillagerAnimations)
	ecs::SetVillagerState(entity, VillagerStates::InHand);
	// Animal::InterfaceSetInMagicHand: off its flock, IN_HAND
	if (Locator::entitiesRegistry::value().AllOf<Animal>(entity))
	{
		ecs::animal_ai::PlaceInHand(entity);
	}
}

} // namespace openblack::ecs::living
