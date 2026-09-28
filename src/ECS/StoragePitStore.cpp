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

#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Registry.h"
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

uint32_t StoragePitStore::AddResource(entt::entity store, ResourceType type, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr || amount == 0)
	{
		return 0;
	}
	const auto& pots = Locator::infoConstants::value().pot;
	uint32_t added = 0;
	const auto fill = [&](entt::entity pile) {
		auto* pot = StorePot(pile);
		if (pot == nullptr || amount == 0)
		{
			return;
		}
		// Pot::JustAddResource 0x66D2B0 clips at maxAmountInPot only when nextPotForResource is set.
		const auto& info = pots.at(static_cast<size_t>(pot->type));
		const uint32_t cap = info.nextPotForResource != PotInfo::_COUNT ? info.maxAmountInPot : 65535u;
		const uint32_t room = cap > pot->amount ? cap - pot->amount : 0u;
		const uint32_t n = std::min(amount, room);
		if (n == 0)
		{
			return;
		}
		pot->amount = static_cast<uint16_t>(pot->amount + n);
		amount -= n;
		added += n;
		archetypes::PotArchetype::SetSize(pile, true);
	};
	if (type == ResourceType::Wood)
	{
		for (const auto pile : pit->woodPiles)
		{
			fill(pile);
		}
	}
	else
	{
		fill(pit->foodPile);
	}
	SyncStoreTotals(store);
	return added;
}

uint32_t StoragePitStore::RemoveResource(entt::entity store, ResourceType type, uint32_t amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	if (pit == nullptr || amount == 0)
	{
		return 0;
	}
	uint32_t removed = 0;
	const auto take = [&](entt::entity pile) {
		auto* pot = StorePot(pile);
		if (pot == nullptr || amount == 0 || pot->amount == 0)
		{
			return;
		}
		const uint32_t n = std::min<uint32_t>(amount, pot->amount);
		pot->amount = static_cast<uint16_t>(pot->amount - n);
		amount -= n;
		removed += n;
		archetypes::PotArchetype::SetSize(pile, true);
	};
	if (type == ResourceType::Wood)
	{
		for (auto it = pit->woodPiles.rbegin(); it != pit->woodPiles.rend(); ++it)
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
