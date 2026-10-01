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

#include "ECS/Components/Life.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
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
	// (inferido) no Life component yet = full life: every object has its life at Object +0x48 (0x402600), and openblack
	// only assigns the component on the first change
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
	if (!registry.Valid(entity))
	{
		return; // already gone (two deaths in one turn)
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} died ({})", registry.AllOf<Villager>(entity) ? "villager" : "animal",
	                   reason);
	// the class's GameThing::ToBeDeleted (vt +0xC): Villager / Animal DeleteDependancys, out of the physics and the
	// registry (ECS/ToBeDeleted.h)
	ecs::ToBeDeleted(entity);
}
