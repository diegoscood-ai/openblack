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
#include <array>
#include <tuple>

#include <glm/vec3.hpp>

#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Common/GameRandom.h"
#include "ECS/Abodes.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/ObjectPoison.h"
#include "ECS/ObjectResources.h"
#include "ECS/Registry.h"
#include "ECS/StoreRules.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "Worship/LeashPosts.h"

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

/// Where the store keeps its pile `index` of a type: food at its model's point index + 5, wood at its point index,
/// through the store's matrix (its place, its turn and its size), as its piles are placed when it is made. Any other
/// type, a point the model lacks or a store whose model is not loaded: the store's own place
glm::vec3 PilePosition(entt::entity store, ResourceType type, size_t index)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& transform = registry.Get<const Transform>(store);
	const auto* mesh = registry.TryGet<const Mesh>(store);
	if ((type != ResourceType::Food && type != ResourceType::Wood) || mesh == nullptr || !Locator::resources::has_value() ||
	    !Locator::resources::value().GetMeshes().Contains(mesh->id))
	{
		return transform.position;
	}
	const auto& points = Locator::resources::value().GetMeshes().Handle(mesh->id)->GetExtraMetrics();
	const auto* abode = registry.TryGet<const Abode>(store);
	const float yAngle = abode != nullptr ? abode->yAngle : 0.0f;
	const auto matrix = affine::PlacementMatrix(transform.position, yAngle, transform.scale.x);
	return worship::leash_posts::Point(matrix, points, type == ResourceType::Food ? index + 5 : index);
}
} // namespace

uint32_t StoragePitStore::AddResource(entt::entity store, ResourceType type, uint32_t amount,
                                      const pot_resource::Dropper& dropper, bool poisoned)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a building site (the pit is not built yet) with WOOD or ANY -> the site's AddResource: an unbuilt storage pit
	// collects its wood on its site
	if (const auto site = abodes::GetBuildingSite(store);
	    site != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
	{
		return building_sites::AddResource(site, type, amount, nullptr, poisoned);
	}
	// only FOOD and WOOD fill piles; (approximate) for any other type the original still runs the pulse test and
	// DoResourceAdding with 0
	if (!registry.AllOf<StoragePit>(store) || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	// (*) the store's total is read after the piles but before DoResourceAdding, so it is still the total from before
	// this call
	const uint32_t before = GetResource(store, type);
	uint32_t added = 0;
	const auto fill = [&](size_t index) {
		// while n != 0
		if (amount == 0)
		{
			return;
		}
		auto slotOf = [&registry, store, type, index]() -> entt::entity& {
			auto& pit = registry.Get<StoragePit>(store);
			return type == ResourceType::Wood ? pit.woodPiles.at(index) : pit.foodPile;
		};
		// a missing pile (one deleted while the store stood) is made again first, empty, at its point of the store and
		// turned with it; it is poisoned if what is put in is
		if (slotOf() == entt::null)
		{
			const auto row = type == ResourceType::Wood
			                     ? static_cast<PotInfo>(static_cast<int32_t>(PotInfo::WoodPile_1) + static_cast<int32_t>(index))
			                     : PotInfo::StoragePitFoodPile;
			const auto* abode = registry.TryGet<const Abode>(store);
			const auto pile = archetypes::PotArchetype::Create(PilePosition(store, type, index),
			                                                   abode != nullptr ? abode->yAngle : 0.0f, row, 0, true);
			slotOf() = pile;
			if (pile != entt::null)
			{
				object_poison::SetPoisoned(pile, poisoned || object_poison::IsPoisoned(pile));
			}
		}
		// only an available pile
		const auto pile = slotOf();
		if (!ecs::IsAvailable(pile) || StorePot(pile) == nullptr)
		{
			return;
		}
		// AddToPotDirect: the pile sound with the n still asked for, the cap, the poison, SetSize
		const uint32_t n = pot_resource::AddToPotDirect(pile, type, amount, poisoned);
		added += n;
		amount -= n;
	};
	const auto fillPiles = [&]() {
		if (type == ResourceType::Wood)
		{
			for (size_t i = 0; i < std::tuple_size_v<decltype(StoragePit::woodPiles)>; ++i) // Wood Pile 1..5
			{
				fill(i);
			}
		}
		else
		{
			fill(0);
		}
		SyncStoreTotals(store);
		return added;
	};
	// DoResourceAdding(type, added, ...). The original fills the piles first and DoResourceAdding's desire "before"
	// still reads the old mirror; openblack's desire reads the piles' total (TownDesire GatherInputs:
	// StoragePitStore::GetResource), so the piles are filled inside its JustAddResource step, between the two desire
	// calls (the same values); its n is what the piles took (justAdd's value). Returns `added`
	object_resources::DoResourceAdding(store, type, amount, dropper, fillPiles);
	const auto town = abode_villagers::TownOf(store);
	// none of it before, something added, a town -> the pulse (before DoResourceAdding in the original; only the
	// town's update reads it, so after it here)
	if (before == 0 && added != 0 && town != entt::null)
	{
		town_queries::Pulse(town);
	}
	return added;
}

std::optional<StoragePitDeed> StoragePitStore::DoCreatureMimicAfterAddingResource(entt::entity store, ResourceType type,
                                                                                  const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!dropper.hasInterface || !registry.Valid(store) || !registry.AllOf<StoragePit>(store))
	{
		return std::nullopt;
	}
	// wood on a pit that is still a building site is the building site's deed, and nothing else is asked
	if (type == ResourceType::Wood && abodes::GetBuildingSite(store) != entt::null)
	{
		return StoragePitDeed::PutWoodInBuildingSite;
	}
	// the pit's own owner is not looked at: only whether what the interface last picked up belonged to the dropper's
	// player. Nothing recorded is never the player's own
	const bool own = dropper.sourceOwner.has_value() && *dropper.sourceOwner == dropper.player;
	if (type == ResourceType::Food)
	{
		if (own)
		{
			return StoragePitDeed::PutFoodInStoragePit;
		}
		// drawn here, before anything checks for a creature
		return game_random::GameRand(2) != 0 ? StoragePitDeed::StealFoodFromStoragePit : StoragePitDeed::StealFoodFromFarm;
	}
	return own ? StoragePitDeed::PutWoodInStoragePit : StoragePitDeed::StealWoodFromStoragePit;
}

uint32_t StoragePitStore::RemoveResource(entt::entity store, ResourceType type, uint32_t amount,
                                         const pot_resource::Dropper& dropper)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* pit = registry.TryGet<const StoragePit>(store);
	// only FOOD and WOOD; any other type 0, with no DoResourceRemoving
	if (pit == nullptr || (type != ResourceType::Food && type != ResourceType::Wood))
	{
		return 0;
	}
	const auto removeFromPiles = [&]() {
		uint32_t removed = 0;
		const auto take = [&](entt::entity pile) {
			// while n != 0: each existing pile's JustRemoveResource (a pit's empty pile stays; the out pointer is not
			// passed)
			if (amount == 0 || StorePot(pile) == nullptr)
			{
				return;
			}
			const uint32_t n = object_resources::RemoveFromPotDirect(pile, amount);
			removed += n;
			amount -= n;
		};
		if (type == ResourceType::Wood)
		{
			for (auto it = pit->woodPiles.rbegin(); it != pit->woodPiles.rend(); ++it) // pile 5 -> 1
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
	// removed != 0 -> DoResourceRemoving(type, removed, ...). Its desire "before" reads the mirror, still the old
	// total: openblack's mirror is the piles' total, so the desire is taken before the piles change. Something comes
	// off exactly when the store holds some and n != 0
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
	// DoResourceRemoving's n is what came off, min(n, the store's total)
	return object_resources::DoResourceRemoving(store, type, std::min(amount, held), dropper, removeFromPiles);
}

void StoragePitStore::RemovePotFromStructure(entt::entity store, entt::entity pile)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* pit = registry.TryGet<StoragePit>(store);
	if (pit == nullptr || pile == entt::null)
	{
		return;
	}
	// the food pile, then each wood pile: the slot holding it takes the pile's amount off the store (DoResourceRemoving
	// of the pile's own type, with no interface) and is emptied, so the next food or wood to reach it makes it again
	const auto leave = [store, pile](auto&& slotOf) {
		if (slotOf() != pile)
		{
			return;
		}
		const auto* pot = StorePot(pile);
		const auto type = pot != nullptr ? Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType
		                                 : ResourceType::Any;
		const uint32_t amount = pot != nullptr ? pot->amount : 0;
		object_resources::DoResourceRemoving(store, type, amount, {}, [&]() {
			slotOf() = entt::null;
			SyncStoreTotals(store);
			return amount;
		});
	};
	leave([&registry, store]() -> entt::entity& { return registry.Get<StoragePit>(store).foodPile; });
	for (size_t i = 0; i < pit->woodPiles.size(); ++i)
	{
		leave([&registry, store, i]() -> entt::entity& { return registry.Get<StoragePit>(store).woodPiles.at(i); });
	}
}

int32_t StoragePitStore::AmountOverMaximum(entt::entity store, ResourceType type)
{
	// WOOD total - 5 x the Wood Pile 1 pot's max; any other type total - the Storage Pit Food Pile's max
	const auto& pots = Locator::infoConstants::value().pot;
	const bool wood = type == ResourceType::Wood;
	const auto pile = wood ? PotInfo::WoodPile_1 : PotInfo::StoragePitFoodPile;
	// the totals of six piles of whole amounts are far inside the 32-bit range
	return static_cast<int32_t>(
	    store_rules::AmountOverMaximum(wood, GetResource(store, type), pots.at(static_cast<size_t>(pile)).maxAmountInPot));
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
