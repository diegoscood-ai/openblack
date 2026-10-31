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
#include <optional>

#include "3D/MapCoords.h"
#include "Creature/CreatureDeeds.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/BuildingSite.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/CreatureMimic.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/FishFarms.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/Scaffolds.h"
#include "ECS/StoragePitStore.h"
#include "ECS/StoreRules.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownStores.h"
#include "ECS/Town/Workshops.h"
#include "ECS/TownDesire.h"
#include "ECS/Trees.h"
#include "ECS/Villager/VillagerResources.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

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

Pot* PotComponent(entt::entity pot)
{
	auto& registry = Entities();
	return pot != entt::null && registry.Valid(pot) ? registry.TryGet<Pot>(pot) : nullptr;
}

/// the town's desire for the resource (wood for anything but FOOD): d = 1 (Wood) or 0 (Food)
float CallResourceDesire(entt::entity town, ResourceType type)
{
	return town_desire::CallDesireFunctionNow(town,
	                                          type != ResourceType::Food ? TownDesireInfo::ForWood : TownDesireInfo::ForFood);
}

/// The town's owner
PlayerNames TownOwner(entt::entity town)
{
	const auto* t = Entities().TryGet<const Town>(town);
	return t != nullptr ? t->owner : PlayerNames::NEUTRAL;
}

/// The row of a mushroom, a magic mushroom or a toadstool (by its row's kind); null for any other thing
const GMobileObjectInfo* MushroomInfo(entt::entity object)
{
	const auto* mobile = Entities().TryGet<const MobileObject>(object);
	if (mobile == nullptr || mobile->type == MobileObjectInfo::None || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& info = Locator::infoConstants::value().mobileObject.at(static_cast<size_t>(mobile->type));
	const auto kind = info.mobileType;
	return kind == MobileObjectInfo::Champi || kind == MobileObjectInfo::MagicMushroom || kind == MobileObjectInfo::Toadstool
	           ? &info
	           : nullptr;
}

/// What an object holds of its own resource, whatever type is asked: a scaffold its wood, a tree or a dead tree its
/// wood, an animal its food, a mushroom its food, a thing of the rock class its wood (rocks hold wood), a fence its
/// wood; nothing for anything else (a fish farm and a big forest included)
uint32_t DefaultResource(entt::entity object)
{
	auto& registry = Entities();
	if (scaffolds::IsScaffold(object))
	{
		return scaffolds::GetDefaultResource(object);
	}
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		return TreeWood(object);
	}
	if (const auto* animal = registry.TryGet<const Animal>(object); animal != nullptr)
	{
		if (!Locator::infoConstants::has_value())
		{
			return 0;
		}
		const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type));
		return store_rules::AnimalFood(info.foodValue, static_cast<uint32_t>(info.foodType));
	}
	if (const auto* mushroom = MushroomInfo(object); mushroom != nullptr)
	{
		return store_rules::MushroomFood(mushroom->foodValue);
	}
	if (const auto wood = object_physics::RockWood(object); wood.has_value())
	{
		return *wood;
	}
	if (physics::from_hand::IsFence(object))
	{
		// the wood value of the fence's own row (every fence the game makes has one)
		const auto& info =
		    Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(registry.Get<const MobileStatic>(object).type));
		const auto* transform = registry.TryGet<const Transform>(object);
		const float scale = transform != nullptr ? transform->scale.x : 1.0f;
		return store_rules::FenceWood(life::LifeOf(object), info.woodValue, scale);
	}
	return 0;
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
	if (const auto* a = registry.TryGet<const Abode>(object); a != nullptr)
	{
		// its own amount of that type
		return type == ResourceType::Food ? a->foodAmount : type == ResourceType::Wood ? a->woodAmount : 0;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
	{
		// a pile linked to its building site: the pile's own amount; part of another structure: that structure's amount
		// (a pit's mirror, its whole total)
		if (building_sites::SiteOfPile(object) != entt::null)
		{
			return PotAmount(object, type);
		}
		if (const auto owner = StoragePitStore::OwnerOf(object); owner != entt::null)
		{
			return StoragePitStore::GetResource(owner, type);
		}
		// a workshop's pile (part of the workshop, not linked to its site): the workshop's amount (its mirror)
		if (const auto workshop = workshops::WorkshopOfPile(object); workshop != entt::null)
		{
			return GetResource(workshop, type);
		}
		return PotAmount(object, type);
	}
	// a worship site: the food in its food pot, and nothing else
	if (registry.AllOf<WorshipSite>(object))
	{
		return type == ResourceType::Food ? ::openblack::worship::site::GetFoodResource(object) : 0;
	}
	// a building site: its pile's (a citadel's: its six piles' together)
	if (registry.AllOf<BuildingSite>(object))
	{
		return building_sites::GetResource(object, type);
	}
	// any other object: its own resource when the type asked is its own, else nothing (a scaffold's is its value x
	// WoodValue). A field is an abode, answered above
	return type == GetResourceType(object) ? DefaultResource(object) : 0;
}

ResourceType GetResourceType(entt::entity object)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return ResourceType::None;
	}
	if (const auto* p = registry.TryGet<const Pot>(object); p != nullptr)
	{
		// its info's type
		return Locator::infoConstants::value().pot.at(static_cast<size_t>(p->type)).resourceType;
	}
	// a scaffold, a tree, a dead tree, a thing of the rock class and a fence hold WOOD; an animal, a mushroom, a field
	// and a fish farm FOOD
	if (scaffolds::IsScaffold(object) || registry.AnyOf<Tree, DeadTree>(object) ||
	    object_physics::RockWood(object).has_value() || physics::from_hand::IsFence(object))
	{
		return ResourceType::Wood;
	}
	if (registry.AnyOf<Animal, Field, FishFarm>(object) || MushroomInfo(object) != nullptr)
	{
		return ResourceType::Food;
	}
	return ResourceType::None;
}

uint32_t PotAmount(entt::entity pot, ResourceType type)
{
	const auto* p = PotComponent(pot);
	if (p == nullptr)
	{
		return 0;
	}
	// 0 unless the type is the pot's own, else its amount
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(p->type));
	return info.resourceType == type ? p->amount : 0;
}

uint32_t RemoveFromPotDirect(entt::entity object, uint32_t amount)
{
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	// n >= the amount -> all of it, the reaction removed, the poison off and the fire deleted; else amount -= n
	uint32_t removed = amount;
	if (amount >= pot->amount)
	{
		removed = pot->amount;
		pot->amount = 0;
		animal_ai::RemovePotReaction(object);
		pot->poisoned = false;
		if (auto* fire = fire::Find(object); fire != nullptr)
		{
			fire::ToBeDeleted(*fire);
		}
	}
	else
	{
		pot->amount -= amount;
	}
	// its size follows the amount
	archetypes::PotArchetype::SetSize(object, true);
	// a structure's pot: still something -> its size again (the same result here). Empty: a pile of a structure stays;
	// any other pile, MagicFood or MagicWood is deleted. A plain pot has no such step: it stays. (the deletion:
	// ecs::ToBeDeleted, out of the physics, the map cells, then the entity)
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	// A building site's pile is part of its building: it stays too, until the building site is deleted and releases it
	// A workshop's pile is part of the workshop, and a worship site's food pot part of the site: they stay too
	if (pot->amount == 0 && info.potType != PotType::Pot && StoragePitStore::OwnerOf(object) == entt::null &&
	    building_sites::SiteOfPile(object) == entt::null && workshops::WorkshopOfPile(object) == entt::null &&
	    resource_stores::WorshipSiteOfFoodPot(object) == entt::null)
	{
		// a food pile closes its speed-up visual first, then the common deletion (on the dead list when the deferral is on).
		// A town's temporary pot that goes stays in Town::temporaryPots until the town's turn
		// (town_stores::ProcessTemporaryPots) finds it unavailable; the temporary store and storage pit lookups ask
		// IsAvailable meanwhile
		pot_resource::SetSpeedUp(object, false);
		ecs::ToBeDeleted(object); // the common deletion: physics, map cells, the entity
	}
	return removed;
}

uint32_t DoResourceRemoving(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                            const std::function<uint32_t()>& justRemove)
{
	// amount >= what it has -> the poison flag would be cleared: no effect (an abode keeps no poison flag; a storage
	// pit ORs with each pile's own: nothing is ever cleared)
	const auto town = abode_villagers::TownOf(abode);
	// the town's desire before the change
	const float before = town != entt::null ? CallResourceDesire(town, type) : 0.0f;
	// the change itself
	const uint32_t removed = justRemove();
	// with an interface and a town
	if (dropper.hasInterface && town != entt::null)
	{
		town_stores::SetGameTurnResourceLastRemoved(town, dropper.player, type);
		const float after = CallResourceDesire(town, type);
		// before - after; the alignment of the town's owner, with -n the amount asked for (literal negation, not verified in
		// game: a huge unsigned n wraps to a positive int, as in the original)
		effects::alignment::UpdateForResource(TownOwner(town), abode, -static_cast<int32_t>(amount), before - after);
	}
	return removed;
}

uint32_t DoResourceAdding(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                          const std::function<uint32_t()>& justAdd)
{
	const auto town = abode_villagers::TownOf(abode);
	// no interface or no town -> the change only
	if (!dropper.hasInterface || town == entt::null)
	{
		return justAdd();
	}
	// the desire before and after the change
	const float before = CallResourceDesire(town, type);
	const uint32_t added = justAdd();
	const float after = CallResourceDesire(town, type);
	// the drop x the modifier for the interface's player's last removal of that type; for the town's belief also x (not
	// the town's owner ? the non-owner multiplier : 1) x the town multiplier
	const float modifier = town_stores::GetGameTurnResourceLastRemovedModifier(town, dropper.player, type);
	const auto& info = Locator::infoConstants::value().town;
	const float nonOwner = dropper.player != TownOwner(town) ? info.multiplierForNonOwnerAddingResource : 1.0f;
	const auto gift = store_rules::GiftToTown(before, after, modifier, nonOwner, info.multiplierForAddingResourceToTown);
	// the interface's player's alignment, with n: an abode's is the amount asked for, which the change returns whole; a
	// pit's is what its piles took, justAdd's value here. The type only feeds the alignment history, not kept
	// (UpdateForResource has no type)
	static_cast<void>(amount);
	effects::alignment::UpdateForResource(dropper.player, abode, static_cast<int32_t>(added), gift.alignmentChange);
	// the town's belief
	town_stores::AddToBelief(town, dropper.player, gift.belief, abode, true, 1);
	// then the creature is told what was done, whatever was added (a storage pit's deed with its draw, or the other
	// abodes' deeds)
	ReportResourceDeed(abode, type, dropper);
	return added;
}

void ReportResourceDeed(entt::entity abode, ResourceType type, const pot_resource::Dropper& dropper)
{
	auto& registry = Entities();
	if (!dropper.hasInterface || abode == entt::null || !registry.Valid(abode))
	{
		return;
	}
	std::optional<creature_watching::Deed> deed;
	if (registry.AllOf<StoragePit>(abode))
	{
		if (const auto pitDeed = StoragePitStore::DoCreatureMimicAfterAddingResource(abode, type, dropper))
		{
			deed = static_cast<creature_watching::Deed>(*pitDeed);
		}
	}
	else
	{
		const auto receiver = registry.AllOf<WorshipSite>(abode) ? creature_watching::Receiver::WorshipSite
		                      : registry.AllOf<Workshop>(abode)  ? creature_watching::Receiver::Workshop
		                                                         : creature_watching::Receiver::Other;
		// the building site test comes first, as the pit's own does
		deed = creature_watching::GiftDeed(
		    {.receiver = receiver, .goingUp = abodes::GetBuildingSite(abode) != entt::null, .resource = type});
	}
	if (deed.has_value())
	{
		creature_mimic::Consider(dropper.player, *deed, abode);
	}
}

uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// the storage pit: the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::RemoveResource(object, type, amount, dropper);
	}
	// a workshop (no building-site redirect): its pile, then its mirror
	if (workshops::IsWorkshop(object))
	{
		return workshops::RemoveResource(object, type, amount, dropper);
	}
	// a worship site: wood or any from its building site while it has one, food from its food pot
	if (registry.AllOf<WorshipSite>(object))
	{
		return ::openblack::worship::site::RemoveResource(object, type, amount);
	}
	if (auto* a = registry.TryGet<Abode>(object); a != nullptr)
	{
		// an abode with a building site and WOOD or ANY -> the site's RemoveResource
		if (a->buildingSite != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
		{
			return building_sites::RemoveResource(a->buildingSite, type, amount);
		}
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// then DoResourceRemoving
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceRemoving(object, type, amount, dropper, [&held, amount]() {
			// min(amount, what it has) off
			const uint32_t removed = std::min(amount, held);
			held -= removed;
			return removed;
		});
	}
	// a building site: its own take (its pile; a workshop's site, the workshop; a citadel's, its piles in order)
	if (registry.AllOf<BuildingSite>(object))
	{
		return building_sites::RemoveResource(object, type, amount);
	}
	// a dead tree: its wood (it shrinks, or goes when all of it is asked); asked for any other type it holds none of
	// it, so it goes and gives nothing
	if (registry.AllOf<DeadTree>(object))
	{
		if (type != ResourceType::Wood)
		{
			DeleteTree(object);
			return 0;
		}
		return RemoveWood(object, amount);
	}
	// a big forest gives only wood: it shrinks, a sapling grows at its edge, or it goes
	if (registry.AllOf<BigForest>(object))
	{
		return type == ResourceType::Wood ? BigForestRemoveWood(object, amount) : 0;
	}
	// a fish farm gives only food, its fish
	if (registry.AllOf<FishFarm>(object))
	{
		return static_cast<uint32_t>(fish_farms::RemoveResource(object, type, static_cast<int32_t>(amount)));
	}
	// a thing of the rock class: its wood (nothing of any other type, so it goes)
	if (const auto wood = object_physics::RockWood(object); wood.has_value())
	{
		return object_physics::TakeFromRock(object, type == ResourceType::Wood ? *wood : 0, amount);
	}
	// any other structure (a citadel's part) with a building site: the site's, whatever the type; without one nothing
	if (!registry.AllOf<Pot>(object))
	{
		const auto site = abodes::GetBuildingSite(object);
		return site != entt::null ? building_sites::RemoveResource(site, type, amount) : 0;
	}
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	// a pile linked to its structure's building site -> the site's RemoveResource (every argument passed on)
	if (const auto site = building_sites::SiteOfPile(object); site != entt::null)
	{
		return building_sites::RemoveResource(site, type, amount);
	}
	// a workshop's pile (nothing over the maximum for a workshop): the pile, the workshop's DoResourceRemoving, then the
	// workshop's RemoveResource for the rest
	if (workshops::WorkshopOfPile(object) != entt::null)
	{
		return workshops::RemoveResourceFromPile(object, type, amount, dropper);
	}
	// a worship site's food pot (nothing over the maximum for a site, and its removal step does nothing): the pot gives
	// what it can of what is asked, whatever the type, and stays; then the site's RemoveResource for the rest (its
	// building site's wood, or its food pot again)
	if (const auto site = resource_stores::WorshipSiteOfFoodPot(object); site != entt::null)
	{
		uint32_t removed = amount != 0 ? RemoveFromPotDirect(object, amount) : 0;
		if (removed < amount)
		{
			removed += ::openblack::worship::site::RemoveResource(site, type, amount - removed);
		}
		return removed;
	}
	const auto owner = StoragePitStore::OwnerOf(object);
	if (owner == entt::null)
	{
		// a pot or pile without a structure: RemoveFromPotDirect, whatever the type (no test)
		return RemoveFromPotDirect(object, amount);
	}
	// a pile of a storage pit: over = the pit's amount above its maximum; the touched pile gives n - min(over, n) when
	// over > 0, else n
	const int32_t over = StoragePitStore::AmountOverMaximum(owner, type);
	const uint32_t mine = store_rules::AskedOfPile(amount, over);
	uint32_t removed = 0;
	// r = what the pile gave, then r != 0 -> the pit's DoResourceRemoving(r). The desire before reads the pit's mirror
	// before the pile changes: openblack's mirror is the piles' total, so it runs around the pile's change (the same
	// values)
	if (mine != 0)
	{
		if (pot->amount != 0)
		{
			// DoResourceRemoving's n is r, what the pile gave
			const uint32_t r = std::min<uint32_t>(mine, pot->amount);
			removed = DoResourceRemoving(owner, type, r, dropper, [object, mine, owner]() {
				const uint32_t n = RemoveFromPotDirect(object, mine);
				StoragePitStore::SyncTotals(owner);
				return n;
			});
		}
		else
		{
			// an empty pile is still asked (poison, reaction and fire cleared, its size); r == 0 -> no DoResourceRemoving
			RemoveFromPotDirect(object, mine);
		}
	}
	// r < n -> r += the pit's RemoveResource(n - r) (pile 5 -> 1, the touched pile again)
	if (removed < amount)
	{
		removed += StoragePitStore::RemoveResource(owner, type, amount - removed, dropper);
	}
	return removed;
}

uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                     bool poisoned, const glm::vec3* position)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// where it is put down, as map coordinates, for the building sites that take it at the pile nearest to it
	const std::optional<map_coords::MapCoords> coords =
	    position != nullptr ? std::optional(map_coords::FromWorld(*position)) : std::nullopt;
	const map_coords::MapCoords* at = coords.has_value() ? &*coords : nullptr;
	// the storage pit (which is also an abode): the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::AddResource(object, type, amount, dropper, poisoned);
	}
	// a workshop (no building-site redirect): its pile and its mirror
	if (workshops::IsWorkshop(object))
	{
		return workshops::AddResource(object, type, amount, poisoned);
	}
	// a worship site: its building site's wood, else its food pot
	if (registry.AllOf<WorshipSite>(object))
	{
		return ::openblack::worship::site::AddResource(object, type, amount, poisoned);
	}
	if (auto* a = AbodeComponent(object); a != nullptr)
	{
		// an abode with a building site and WOOD or ANY -> the site's AddResource (no position: a citadel building site would
		// add nothing)
		if (a->buildingSite != entt::null && (type == ResourceType::Wood || type == ResourceType::Any))
		{
			return building_sites::AddResource(a->buildingSite, type, amount, nullptr, poisoned);
		}
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// else DoResourceAdding: amount added, no cap (poisoned is not kept by an abode)
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceAdding(object, type, amount, dropper, [&held, amount]() {
			held += amount;
			return amount;
		});
	}
	// a citadel's part (its heart) with a building site -> the site's AddResource with where it is put down, whatever the
	// type; else nothing
	if (const auto* part = registry.TryGet<const CitadelPartBuild>(object); part != nullptr)
	{
		return part->buildingSite != entt::null ? building_sites::AddResource(part->buildingSite, type, amount, at, poisoned)
		                                        : 0;
	}
	if (registry.AllOf<Pot>(object))
	{
		// a pile linked to its structure's building site -> the site's AddResource (every argument passed on, where it is
		// put down too: a citadel's site gives it to its pile nearest that point)
		if (const auto site = building_sites::SiteOfPile(object); site != entt::null)
		{
			return building_sites::AddResource(site, type, amount, at, poisoned);
		}
		// part of a structure not linked to its site -> the structure's AddResource (every argument passed on): a
		// workshop's pile -> the workshop's AddResource
		if (const auto workshop = workshops::WorkshopOfPile(object); workshop != entt::null)
		{
			return workshops::AddResource(workshop, type, amount, poisoned);
		}
		// a worship site's food pot -> the worship site's AddResource
		if (const auto site = resource_stores::WorshipSiteOfFoodPot(object); site != entt::null)
		{
			return ::openblack::worship::site::AddResource(site, type, amount, poisoned);
		}
		// any other pot or pile (pot_resource): a pile of a storage pit passes the interface and the poison on to the pit
		return pot_resource::PotStructureAddResource(object, type, amount, poisoned, dropper);
	}
	// a scaffold of a building site -> the site's AddResource (every argument passed on); else nothing
	if (const auto* scaffold = registry.TryGet<const Scaffold>(object); scaffold != nullptr)
	{
		return scaffold->site != entt::null ? building_sites::AddResource(scaffold->site, type, amount, at, poisoned) : 0;
	}
	// a villager picks the food or wood up into its hands, with no limit (poisoned food poisons it), and reports that
	// it took nothing
	if (registry.AllOf<Villager>(object))
	{
		return villager::AddResourceToVillager(object, type, amount, poisoned);
	}
	return 0;
}

bool IsPileResource(entt::entity object)
{
	if (object == entt::null || !Entities().Valid(object))
	{
		return false;
	}
	const auto* info = object::PotInfoOf(object);
	return info != nullptr && info->potType != PotType::Pot;
}

PlayerNames PlayerOfPile(entt::entity pile)
{
	const auto* pot = PotComponent(pile);
	if (pot == nullptr)
	{
		return PlayerNames::NEUTRAL;
	}
	// a pile of a storage pit answers with the pit's player: the pit's town's owner, the neutral player without a town
	if (const auto pit = StoragePitStore::OwnerOf(pile); pit != entt::null)
	{
		const auto town = abode_villagers::TownOf(pit);
		return town != entt::null ? TownOwner(town) : PlayerNames::NEUTRAL;
	}
	// a worship site's food pot answers with the site's player, its citadel's (the piles of workshops and building
	// sites answer with their own player, as magic wood does)
	if (const auto site = resource_stores::WorshipSiteOfFoodPot(pile); site != entt::null)
	{
		if (const auto* worship = Entities().TryGet<const WorshipSite>(site); worship != nullptr)
		{
			return worship->player;
		}
	}
	return pot->owner;
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
		// an available pile of the food one or the five wood ones whose poison flag is set
		const auto poisoned = [&registry](entt::entity pile) {
			const auto* pot = ecs::IsAvailable(pile) ? registry.TryGet<const Pot>(pile) : nullptr;
			return pot != nullptr && pot->poisoned;
		};
		return poisoned(pit->foodPile) || std::any_of(pit->woodPiles.begin(), pit->woodPiles.end(), poisoned);
	}
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->poisoned;
	}
	// a villager, an animal or a creature its poisoned bit
	if (registry.AnyOf<Villager, Animal, Creature>(object))
	{
		return life::IsPoisoned(object);
	}
	// a toadstool is poisoned by its kind; every other mobile object never is
	if (const auto* mobile = registry.TryGet<const MobileObject>(object))
	{
		return mobile->type == MobileObjectInfo::Toadstool;
	}
	return false; // a plain object is never poisoned
}

entt::entity SplitOffPile(entt::entity pile, uint32_t most)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& pot = registry.Get<Pot>(pile);
	// at most the pile's resource (a store pile answers with its store's total, as the hand reads it)
	const auto store = ecs::StoragePitStore::OwnerOf(pile);
	const auto& pots = Locator::infoConstants::value().pot;
	const auto resource = pots[static_cast<size_t>(pot.type)].resourceType;
	const uint32_t available = store != entt::null ? ecs::StoragePitStore::GetResource(store, resource) : pot.amount;
	const auto amount = std::min<uint32_t>(most, available);
	if (amount == 0)
	{
		return entt::null;
	}
	// removed from the store, or from the pile (approximate: the amount and the sink offset, as HandHolding does;
	// the pile's own empty handling is not ported)
	if (store != entt::null)
	{
		ecs::StoragePitStore::RemoveResource(store, resource, amount);
	}
	else
	{
		pot.amount -= amount;
		ecs::archetypes::PotArchetype::SetSize(pile, true);
	}
	// a new pot of the hand's own pile kind (food: HAND_FOOD, wood: HAND_WOOD), at the pile's position
	const auto position = registry.Get<const Transform>(pile).position;
	const auto handType = resource == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
	return ecs::archetypes::PotArchetype::Create(position, 0.0f, handType, static_cast<int32_t>(amount));
}
} // namespace openblack::ecs::object_resources
