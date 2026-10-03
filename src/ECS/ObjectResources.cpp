/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectResources.h"

#include <algorithm>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Villager.h"
#include "ECS/Life.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "Locator.h"

namespace openblack::ecs::object_resources
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

Abode* AbodeComponent(entt::entity abode)
{
	auto& registry = Entities();
	return abode != entt::null && registry.Valid(abode) ? registry.TryGet<Abode>(abode) : nullptr;
}
} // namespace

uint32_t GetResource(entt::entity object, ResourceType type)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::GetResource(object, type);
	}
	const auto* a = registry.TryGet<const Abode>(object);
	if (a == nullptr)
	{
		return 0;
	}
	// 0x404D30: +0xBC[type]
	return type == ResourceType::Food ? a->foodAmount : type == ResourceType::Wood ? a->woodAmount : 0;
}

uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// StoragePit::RemoveResource 0x7332A0 (vt +0xA0 of the storage pit): the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::RemoveResource(object, type, amount);
	}
	if (!registry.AllOf<Abode>(object) || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	// Abode::RemoveResource 0x404F10: +0x74 (the building site) with WOOD or -2 -> the site's RemoveResource.
	// TODO(V6): openblack's abodes have no building site; then DoResourceRemoving 0x404F60 (vt +0x8E8)
	// 0x404F6E..0x404F82: amount >= GetResource -> SetPoisoned(0) (GameThingWithPos keeps no poison flag: nothing)
	// 0x404F88..0x404FA0: the town's CallDesireFunction(type != 0) before the removal (its value is kept only for the
	// GInterfaceStatus branch, 0x404FE9)
	if (const auto town = abode_villagers::TownOf(object); town != entt::null)
	{
		town_desire::CallDesireFunctionNow(town, type != ResourceType::Food ? TownDesireInfo::ForWood : TownDesireInfo::ForFood);
	}
	// 0x404FB1 JustRemoveResource 0x404D60: min(amount, +0xBC[type]) off. The GInterfaceStatus branch (the hand:
	// SetGameTurnResourceLastRemoved, a second CallDesireFunction, GAlignment::Update, 0x404FC7..0x40500A) is not a
	// villager's
	auto& a = registry.Get<Abode>(object);
	auto& held = type == ResourceType::Food ? a.foodAmount : a.woodAmount;
	const uint32_t removed = std::min(amount, held);
	held -= removed;
	return removed;
}

uint32_t AddResource(entt::entity abode, ResourceType type, uint32_t amount)
{
	auto* a = AbodeComponent(abode);
	if (a == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	// Abode::AddResource 0x404D90 (no +0x74) -> DoResourceAdding 0x404DF0 without a GInterfaceStatus (0x404E01 je):
	// JustAddResource 0x404D40, +0xBC[type] += amount
	auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
	held += amount;
	return amount;
}

bool IsPoisoned(entt::entity object)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return false;
	}
	if (const auto* pit = registry.TryGet<const StoragePit>(object))
	{
		// StoragePit::IsPoisoned 0x7336B0 = IsPoisonedResource(0) || IsPoisonedResource(1) 0x733550: an available pile
		// of the food one (+0xC4) or the five wood ones (+0xC8) whose IsPoisoned (Pot +0x74 bit 0) is set
		const auto poisoned = [&registry](entt::entity pile) {
			const auto* pot = pile != entt::null && registry.Valid(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
			return pot != nullptr && pot->poisoned;
		};
		return poisoned(pit->foodPile) || std::any_of(pit->woodPiles.begin(), pit->woodPiles.end(), poisoned);
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->poisoned; // Pot::IsPoisoned 0x55D4E0
	}
	if (registry.AnyOf<Villager>(object))
	{
		return life::IsPoisoned(object);
	}
	return false; // GameThingWithPos::IsPoisoned 0x402400: `xor al, al`
}
} // namespace openblack::ecs::object_resources
