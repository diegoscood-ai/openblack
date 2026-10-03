/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StoragePitStore.h"

#include <algorithm>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeVillagers.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
void SyncStoreTotals(entt::entity store)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* abode = registry.TryGet<Abode>(store);
	if (abode == nullptr)
	{
		return;
	}
	abode->woodAmount = StoragePitStore::GetResource(store, ResourceType::Wood);
	abode->foodAmount = StoragePitStore::GetResource(store, ResourceType::Food);
}

Pot* StorePot(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	return pile != entt::null && registry.Valid(pile) ? registry.TryGet<Pot>(pile) : nullptr;
}
} // namespace

uint32_t StoragePitStore::AddResource(entt::entity store, ResourceType type, uint32_t amount,
                                      const pot_resource::Dropper& dropper, bool poisoned)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	// 0x732F67..0x732F99: the +0x74 building-site branch (WOOD or -2) is TODO(V6). 0x733083: only FOOD and WOOD fill
	// piles; (approximate) for any other type the original still runs the pulse test and DoResourceAdding with 0
	if (pit == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	// (*) 0x73315E: GetResource (vt +0x98, the mirror) is read after the piles but before DoResourceAdding, so it is
	// still the total from before this call
	const uint32_t before = GetResource(store, type);
	uint32_t added = 0;
	const auto fill = [&](entt::entity pile) {
		// 0x732FBA / 0x733094: while n != 0. A missing pile is created first (Pot::Create 0x66CF10 at GetResourcePos,
		// 0x732FF8 / 0x7330D2): (approximate) openblack creates all six with the pit (AbodeArchetype) and never deletes
		// them, so there is none to make. 0x73302C..0x733037: only an available pile
		if (amount == 0 || StorePot(pile) == nullptr)
		{
			return;
		}
		// JustAddResource (vt +0x8C, PileResource 0x66D330 -> Pot 0x66D2B0): the pile sound with the n still asked for,
		// the cap, the poison, SetSize (0x73304B..0x733061)
		const uint32_t n = pot_resource::JustAddResource(pile, type, amount, poisoned);
		added += n;
		amount -= n;
	};
	const auto fillPiles = [&]() {
		if (type == ResourceType::Wood)
		{
			for (const auto pile : pit->woodPiles) // +0xC8 -> +0xD8, Wood Pile 1..5
			{
				fill(pile);
			}
		}
		else
		{
			fill(pit->foodPile); // +0xC4
		}
		SyncStoreTotals(store);
		return added;
	};
	// 0x7331B1: DoResourceAdding(type, added, IS, poisoned, pos, k) (vt +0x8E4). The original fills the piles first and
	// DoResourceAdding's desire "before" (0x404E22) still reads the old mirror; openblack's desire reads the piles'
	// total (TownDesire GatherInputs: StoragePitStore::GetResource), so the piles are filled inside its JustAddResource
	// step, between the two desire calls (the same values); its n is what the piles took (justAdd's value). Returns
	// `added` (0x7331B9)
	object_resources::DoResourceAdding(store, type, amount, dropper, fillPiles);
	const auto town = abode_villagers::TownOf(store);
	// 0x73316D..0x73318D: none of it before, something added, a town -> the pulse +0x5E8 = 1, +0x5EC = 0 (before
	// DoResourceAdding in the original; only Town::Process reads it, so after it here)
	if (before == 0 && added != 0 && town != entt::null)
	{
		if (auto* t = registry.TryGet<Town>(town); t != nullptr)
		{
			t->buildPulse = 1;
			t->buildPulsePrevious = 0;
		}
	}
	return added;
}

uint32_t StoragePitStore::RemoveResource(entt::entity store, ResourceType type, uint32_t amount,
                                         const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	// 0x7332E9: only FOOD and WOOD; any other type 0, with no DoResourceRemoving
	if (pit == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	const auto removeFromPiles = [&]() {
		uint32_t removed = 0;
		const auto take = [&](entt::entity pile) {
			// while n != 0: each existing pile's JustRemoveResource (vt +0x90 = PotStructure 0x66D9B0, a pit's empty
			// pile stays; the out pointer is not passed, 0x7332C6)
			if (amount == 0 || StorePot(pile) == nullptr)
			{
				return;
			}
			const uint32_t n = object_resources::JustRemoveFromPot(pile, amount);
			removed += n;
			amount -= n;
		};
		if (type == ResourceType::Wood)
		{
			for (auto it = pit->woodPiles.rbegin(); it != pit->woodPiles.rend(); ++it) // +0xD8 -> +0xC8, pile 5 -> 1
			{
				take(*it);
			}
		}
		else
		{
			take(pit->foodPile);
		}
		SyncStoreTotals(store);
		return removed;
	};
	// 0x73331D..0x733337: removed != 0 -> DoResourceRemoving(type, removed, IS, out) (vt +0x8E8). Its desire "before"
	// reads the mirror, still the old total: openblack's mirror is the piles' total, so the desire is taken before the
	// piles change. Something comes off exactly when the store holds some and n != 0
	const uint32_t held = GetResource(store, type);
	if (amount == 0)
	{
		return 0;
	}
	if (held == 0)
	{
		// nothing comes off, but each existing pile is still asked while n != 0 (an empty pile: poison, reaction and
		// fire cleared again, SetSize); removed == 0 -> no DoResourceRemoving
		return removeFromPiles();
	}
	// DoResourceRemoving's n is what came off (0x733329: the removed total), min(n, the store's total)
	return object_resources::DoResourceRemoving(store, type, std::min(amount, held), dropper, removeFromPiles);
}

int32_t StoragePitStore::AmountOverMaximum(entt::entity store, ResourceType type)
{
	// StoragePit::CalulateAmountOverMaximum 0x733260: WOOD mirror - 5 x GPotInfo[3] max ([0xD4CB48]); any other type
	// mirror[type] - GPotInfo[2] max ([0xD4CA04])
	const auto& pots = Locator::infoConstants::value().pot;
	const int32_t total = static_cast<int32_t>(GetResource(store, type));
	if (type == ResourceType::Wood)
	{
		return total - 5 * static_cast<int32_t>(pots.at(static_cast<size_t>(PotInfo::WoodPile_1)).maxAmountInPot);
	}
	return total - static_cast<int32_t>(pots.at(static_cast<size_t>(PotInfo::StoragePitFoodPile)).maxAmountInPot);
}

void StoragePitStore::SyncTotals(entt::entity store)
{
	SyncStoreTotals(store);
}

uint32_t StoragePitStore::GetResource(entt::entity store, ResourceType type)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr)
	{
		return 0;
	}
	uint32_t total = 0;
	if (type == ResourceType::Wood)
	{
		for (const auto pile : pit->woodPiles)
		{
			if (const auto* pot = StorePot(pile); pot != nullptr)
			{
				total += pot->amount;
			}
		}
	}
	else if (const auto* pot = StorePot(pit->foodPile); pot != nullptr)
	{
		total += pot->amount;
	}
	return total;
}

entt::entity StoragePitStore::OwnerOf(entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	entt::entity owner = entt::null;
	registry.Each<const StoragePit>([&](entt::entity store, const StoragePit& pit) {
		if (pit.foodPile == pile || std::find(pit.woodPiles.begin(), pit.woodPiles.end(), pile) != pit.woodPiles.end())
		{
			owner = store;
		}
	});
	return owner;
}
