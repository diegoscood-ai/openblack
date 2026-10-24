/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Life.h"

#include <optional>
#include <utility>

#include <spdlog/spdlog.h>

#include "ECS/Abodes.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerSpeed.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs::components;

float openblack::ecs::life::LifeOf(entt::entity entity)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return villager->life;
	}
	// a creature's life is its body's, the one its fights and its sleep change
	if (const auto* needs = registry.TryGet<const CreatureNeeds>(entity))
	{
		return needs->needs.life;
	}
	if (const auto* life = registry.TryGet<const Life>(entity))
	{
		return life->value;
	}
	// (inferred) no Life component yet = full life: every object has a life in the original, and openblack only
	// assigns the component on the first change
	return 1.0f;
}

void openblack::ecs::life::SetLife(entt::entity entity, float life)
{
	// the original refuses a life at or below 0.01 for an indestructible object, and for a thing a script holds while a
	// script owns the widescreen bars, wherever the life is set: only a script's SET_PROPERTY keeps it here
	SetLifeUnlessProtected(entity, life, false);
}

void openblack::ecs::life::SetLifeUnlessProtected(entt::entity entity, float life, bool protectedFromDeath)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a protected thing keeps its life against one of a hundredth or less (or not a number)
	const bool kept = protectedFromDeath && !(life > k_LowestLifeOfAProtectedThing);
	if (auto* villager = registry.TryGet<Villager>(entity))
	{
		// a villager's town counts it among its injured as its life crosses the mark, before the life is written, and
		// even when the life is then kept. A count that goes below 0 stays there, as the original's does
		if (const int change = openblack::physics::living::InjuredChange(villager->life, life); change != 0)
		{
			if (auto* town = ecs::IsAvailable(villager->town) ? registry.TryGet<Town>(villager->town) : nullptr)
			{
				town->injured += change;
			}
		}
		if (!kept)
		{
			villager->life = life;
		}
		return;
	}
	if (kept)
	{
		return;
	}
	// (looked up through the const registry, which makes no storage for a land with no creature)
	if (std::as_const(registry).AllOf<CreatureNeeds>(entity))
	{
		registry.Get<CreatureNeeds>(entity).needs.life = life;
		return;
	}
	auto& component = registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity);
	component.value = life;
}

namespace
{
/// Whose life a change of the entity's goes to (life::LifeTarget)
entt::entity LifeTargetOf(entt::entity entity)
{
	const auto* totem = std::as_const(Locator::entitiesRegistry::value()).TryGet<const VillageTotem>(entity);
	return openblack::ecs::life::LifeTarget(entity, totem != nullptr, totem != nullptr ? totem->townCentre : entt::null);
}
} // namespace

float openblack::ecs::life::ReduceLife(entt::entity entity, float amount)
{
	// a village totem's damage is its town centre's
	if (const auto centre = LifeTargetOf(entity); centre != entity)
	{
		return abodes::ReduceLife(centre, amount, std::nullopt);
	}
	const float life = LifeOf(entity);
	SetLife(entity, life < amount ? 0.0f : life - amount);
	return LifeOf(entity);
}

float openblack::ecs::life::IncreaseLife(entt::entity entity, float amount)
{
	// a village totem's repair is its town centre's
	if (const auto centre = LifeTargetOf(entity); centre != entity)
	{
		return abodes::IncreaseLife(centre, amount);
	}
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

bool openblack::ecs::life::IsPoisoned(entt::entity entity)
{
	// the living's poisoned bit
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<Poisoned>(entity);
}

void openblack::ecs::life::SetPoisoned(entt::entity entity, bool poisoned)
{
	// Only a Living has the bit: an object's own poisoned flag (the pots', components::Pot::poisoned) is not this
	// one
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AnyOf<Villager, Animal, Creature>(entity))
	{
		return;
	}
	if (poisoned == registry.AllOf<Poisoned>(entity))
	{
		return;
	}
	if (poisoned)
	{
		registry.Assign<Poisoned>(entity);
	}
	else
	{
		registry.Remove<Poisoned>(entity);
	}
}

void openblack::ecs::life::TakePoisonedResource(entt::entity living)
{
	// adding and taking a poisoned resource both just SetPoisoned(1)
	SetPoisoned(living, true);
}

float openblack::ecs::life::HungerLifeLoss(float food, float hungryForFood, float hungerToLifeMultiplier)
{
	// 1 - food / hungryForFood, then the *bigger* of that and 1 (kept only when it is above 1), x
	// hungerToLifeMultiplier. food is clamped to >= 0 before, so the first term never wins and the loss is the
	// multiplier alone; it is written out as the original computes it
	const float hunger = 1.0f - food / hungryForFood;
	return (hunger > 1.0f ? hunger : 1.0f) * hungerToLifeMultiplier;
}

float openblack::ecs::life::ProcessPoison(entt::entity villager)
{
	if (!IsPoisoned(villager))
	{
		return 0.0f;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	const auto* info = ecs::VillagerInfoOf(villager);
	if (v == nullptr || info == nullptr)
	{
		return 0.0f; // the animals' poison: the hunger check is the villagers' (Animal has no belly)
	}
	const float loss = HungerLifeLoss(v->food, info->hungryForFood, info->hungerToLifeMultiplier);
	const float before = LifeOf(villager);
	ReduceLife(villager, loss); // no player
	return before - LifeOf(villager);
}

void openblack::ecs::life::Kill(entt::entity entity, const char* reason)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity))
	{
		return; // already gone or marked (two deaths in one turn)
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Physics: {} died ({})", registry.AllOf<Villager>(entity) ? "villager" : "animal",
	                   reason);
	// the class's deletion: the villager's / animal's DeleteDependants, out of the physics and the registry
	// (ECS/ToBeDeleted.h)
	ecs::ToBeDeleted(entity);
}
