/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectPoison.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::object_poison
{
using components::Animal;
using components::Creature;
using components::MobileObject;
using components::Pot;
using components::StoragePit;
using components::Villager;

namespace
{
/// The villagers, the animals and the creatures keep their poison as a bit of their own
bool IsLiving(const Registry& registry, entt::entity thing)
{
	return registry.AnyOf<Villager, Animal, Creature>(thing);
}
} // namespace

bool IsPoisoned(entt::entity thing)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	if (IsLiving(registry, thing))
	{
		return life::IsPoisoned(thing);
	}
	// a toadstool is poisoned by its kind; every other mobile object never is
	if (const auto* mobile = registry.TryGet<const MobileObject>(thing))
	{
		return mobile->type == MobileObjectInfo::Toadstool;
	}
	// a pot or a pile its own flag, a storage pit its piles; anything else is never poisoned
	return object_resources::IsPoisoned(thing);
}

void SetPoisoned(entt::entity thing, bool poisoned)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return;
	}
	if (IsLiving(registry, thing))
	{
		life::SetPoisoned(thing, poisoned);
		return;
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(thing))
	{
		// only the food pile, and only ever poisoned: it keeps a poison it already has
		if (IsAvailable(pit->foodPile))
		{
			if (auto* pile = registry.TryGet<Pot>(pit->foodPile))
			{
				pile->poisoned = poisoned || pile->poisoned;
			}
		}
		return;
	}
	if (auto* pot = registry.TryGet<Pot>(thing))
	{
		pot->poisoned = poisoned;
	}
}
} // namespace openblack::ecs::object_poison
