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
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/VillagerSpeed.h"
#include "Help/HelpSystem.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Physics/LivingRules.h"
#include "ScriptHeaders/ScriptPropertyRules.h"

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
	// the original refuses a life at or below 0.01 wherever a life is set, for an indestructible thing and for a thing a
	// script holds while a script owns the widescreen bars
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* helpSystem = help::Get();
	const bool valid = registry.Valid(entity);
	SetLifeUnlessProtected(entity, life,
	                       ProtectedFromDeath(valid && registry.AllOf<Indestructible>(entity),
	                                          valid && ecs::script_held::IsInScript(entity),
	                                          helpSystem != nullptr && helpSystem->IsScriptWideScreen()));
}

float openblack::ecs::life::ClassLifeOf(entt::entity entity)
{
	if (!object_resources::IsPileResource(entity))
	{
		return LifeOf(entity);
	}
	// a pile's life is its amount over the most its kind holds
	const auto& pot = std::as_const(Locator::entitiesRegistry::value()).Get<const Pot>(entity);
	return script::property_rules::PileLife(pot.amount, object::PotInfoOf(entity)->maxAmountInPot);
}

void openblack::ecs::life::SetClassLife(entt::entity entity, float life)
{
	if (!object_resources::IsPileResource(entity))
	{
		SetLife(entity, life);
		return;
	}
	// a pile turns the life into its amount: a lower one takes the difference times the most its kind holds away, a
	// higher one adds as much, poisoned as the pile is
	using script::property_rules::PileHealthWay;
	const uint32_t most = object::PotInfoOf(entity)->maxAmountInPot;
	const auto change = script::property_rules::PileChangeForHealth(ClassLifeOf(entity), life, most);
	if (change.way == PileHealthWay::Remove)
	{
		object_resources::RemoveResource(entity, object_resources::GetResourceType(entity), change.amount);
	}
	else if (change.way == PileHealthWay::Add)
	{
		const bool poisoned = object_resources::IsPoisoned(entity);
		object_resources::AddResource(entity, object_resources::GetResourceType(entity), change.amount, {}, poisoned);
	}
}

namespace
{
/// How the entity's class takes a new life (life::LifeSetter): a pile, a tree (a magic tree too), or the object's own
openblack::ecs::life::LifeSetter LifeSetterOf(entt::entity entity)
{
	using openblack::ecs::life::LifeSetter;
	if (openblack::ecs::object_resources::IsPileResource(entity))
	{
		return LifeSetter::Pile;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(entity) && registry.AllOf<Tree>(entity) ? LifeSetter::Tree : LifeSetter::Object;
}
} // namespace

void openblack::ecs::life::SetLifeUnlessProtected(entt::entity entity, float life, bool protectedFromDeath)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a protected thing keeps its life against one of a hundredth or less (or not a number), unless its class sets its
	// life without the object's setter
	const bool kept = protectedFromDeath && KeepsLife(true, LifeSetterOf(entity), life);
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
/// Whether the entity is a creature, whose fight health its life changes reach
bool IsACreature(entt::entity entity)
{
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return registry.Valid(entity) && registry.AllOf<Creature>(entity);
}

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
	// A creature with an opponent loses as much fight health first, which can knock it out
	if (IsACreature(entity) && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().LoseFightHealth(entity, amount);
	}
	// read and written as the thing's class gives its life (a pile's is its amount)
	const float life = ClassLifeOf(entity);
	SetClassLife(entity, life < amount ? 0.0f : life - amount);
	// a pile that is part of nothing goes as it is emptied: its life is then none
	if (!std::as_const(Locator::entitiesRegistry::value()).Valid(entity))
	{
		return 0.0f;
	}
	return ClassLifeOf(entity);
}

float openblack::ecs::life::IncreaseLife(entt::entity entity, float amount)
{
	// a village totem's repair is its town centre's
	if (const auto centre = LifeTargetOf(entity); centre != entity)
	{
		return abodes::IncreaseLife(centre, amount);
	}
	// A creature with an opponent gains as much fight health first, whatever its life
	if (IsACreature(entity) && Locator::creatureFightSystem::has_value())
	{
		Locator::creatureFightSystem::value().GainFightHealth(entity, amount);
	}
	// read and written as the thing's class gives its life (a pile's is its amount)
	const float life = ClassLifeOf(entity);
	if (life + amount > 1.0f)
	{
		amount = 1.0f - life;
	}
	if (amount != 0.0f)
	{
		SetClassLife(entity, life + amount);
	}
	return ClassLifeOf(entity);
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
