/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Life.h"

#include <spdlog/spdlog.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

float openblack::ecs::life::LifeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return villager->life;
	}
	if (const auto* life = registry.TryGet<const Life>(entity))
	{
		return life->value;
	}
	return 1.0f;
}

void openblack::ecs::life::SetLife(entt::entity entity, float life)
{
	// Object::SetLife 0x63A140 keeps the life above 0.01 when flag 0x40 (or 0x200 in some interface state) is set:
	// UNVERIFIED which objects those are, not ported. Villager::SetLife 0x756B40 also counts the town's villagers under
	// 0.7 life (Town +0x714, fn_00756BC0 / fn_00756BD0): openblack's Town has no such count yet.
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* villager = registry.TryGet<Villager>(entity))
	{
		villager->life = life;
		return;
	}
	auto& component = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
	component.value = life;
}

float openblack::ecs::life::ReduceLife(entt::entity entity, float amount)
{
	const float life = LifeOf(entity);
	SetLife(entity, life < amount ? 0.0f : life - amount);
	return LifeOf(entity);
}

float openblack::ecs::life::IncreaseLife(entt::entity entity, float amount)
{
	const float life = LifeOf(entity);
	if (life + amount > 1.0f)
	{
		amount = 1.0f - life;
	}
	if (amount != 0.0f)
	{
		SetLife(entity, life + amount);
	}
	return LifeOf(entity);
}

void openblack::ecs::life::Kill(entt::entity entity, const char* reason)
{
	auto& registry = Locator::entitiesRegistry::value();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} died ({})", registry.AllOf<Villager>(entity) ? "villager" : "animal",
	                   reason);
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		if (auto* abode = registry.TryGet<Abode>(villager->abode))
		{
			abode->inhabitants.erase(entity);
		}
		if (auto* town = registry.TryGet<Town>(villager->town))
		{
			town->homelessVillagers.erase(entity);
		}
	}
	physics::PhysicsObjects::RemoveObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
