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

#include "ECS/AnimalAI.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Fire/FireEffect.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/StoragePitStore.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownStores.h"
#include "InfoConstants.h"
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

Pot* PotComponent(entt::entity pot)
{
	auto& registry = Entities();
	return pot != entt::null && registry.Valid(pot) ? registry.TryGet<Pot>(pot) : nullptr;
}

/// TownDesire::CallDesireFunction 0x745D80 (type != FOOD) of the abode's town: d = 1 (Wood) or 0 (Food), 0x404E16 /
/// 0x404F93
float CallResourceDesire(entt::entity town, ResourceType type)
{
	return town_desire::CallDesireFunctionNow(town, type != ResourceType::Food ? TownDesireInfo::ForWood : TownDesireInfo::ForFood);
}

/// The town's owner (Town::GetPlayer, Container vt +0x1C: +0x2C)
PlayerNames TownOwner(entt::entity town)
{
	const auto* t = Entities().TryGet<const Town>(town);
	return t != nullptr ? t->owner : PlayerNames::NEUTRAL;
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
		// 0x404D30: +0xBC[type]
		return type == ResourceType::Food ? a->foodAmount : type == ResourceType::Wood ? a->woodAmount : 0;
	}
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr)
	{
		// PotStructure::GetResource 0x66EF00: IsPartOfStructure (vt +0x860, 0x66DA00: +0x78 if available) -> that
		// structure's GetResource (a pit's mirror, its whole total; 0x66EF2F). The building-site test 0x66EF12..0x66EF24
		// (site +0x74 IsLinkedToThisBuildingSite) is TODO(V6)
		if (const auto owner = StoragePitStore::OwnerOf(object); owner != entt::null)
		{
			return StoragePitStore::GetResource(owner, type);
		}
		// Pot::JustGetResource 0x66D390: 0 unless the type is the pot's own (GetResourceType vt +0x690), else +0x70
		const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
		return info.resourceType == type ? pot->amount : 0;
	}
	return 0;
}

uint32_t JustRemoveFromPot(entt::entity object, uint32_t amount)
{
	auto& registry = Entities();
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	// Pot::JustRemoveResource 0x66D410: n >= +0x70 (0x66D41B) -> n = +0x70, +0x70 = 0, RemoveReaction 0x66D6A0, poison
	// off (0x66D42D) and the fire (+0x44) deleted (0x66D431..0x66D43F); else +0x70 -= n
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
		pot->amount = static_cast<uint16_t>(pot->amount - amount);
	}
	// SetSize (vt +0x85C, 0x66D451)
	archetypes::PotArchetype::SetSize(object, true);
	// PotStructure::JustRemoveResource 0x66D9B0: still something -> SetSize again (0x66D9EA, the same result here). Empty:
	// a pile of a structure (+0x78, 0x66D9D3) stays; any other pot, pile, MagicFood or MagicWood is deleted (ToBeDeleted
	// vt +0xC, 0x66D9E0). The Pot class (Pot::RemoveResource 0x66D3F0 -> 0x66D410) has no such step: it stays.
	// (approximate) the deletion as HandResources does it: out of the map cells, then the entity
	const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
	if (pot->amount == 0 && info.potType != PotType::Pot && StoragePitStore::OwnerOf(object) == entt::null)
	{
		// PileFood::ToBeDeleted 0x66E100 closes its +0xB8 speed-up visual first. (approximate) the original defers the
		// deletion (ToBeDeleted); openblack destroys it at once. A town's temporary pot that goes leaves a stale entity in
		// Town::temporaryPots, which town_stores treats as not available (registry.Valid)
		pot_resource::SetSpeedUp(object, false);
		map_cells::RemoveMapObject(object);
		registry.Destroy(object);
	}
	return removed;
}

uint32_t DoResourceRemoving(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                            const std::function<uint32_t()>& justRemove)
{
	// 0x404F6E..0x404F82: amount >= GetResource -> SetPoisoned(0): no effect (an abode keeps no poison flag;
	// StoragePit::SetPoisoned 0x7335D0 ORs with each pile's own: nothing is ever cleared, V5_resources_spec §C.2)
	const auto town = abode_villagers::TownOf(abode);
	// 0x404F88..0x404FA0: the town's desire before the change
	const float before = town != entt::null ? CallResourceDesire(town, type) : 0.0f;
	// 0x404FB1: JustRemoveResource (vt +0x90)
	const uint32_t removed = justRemove();
	// 0x404FBB..0x405005: with an interface and a town
	if (dropper.hasInterface && town != entt::null)
	{
		town_stores::SetGameTurnResourceLastRemoved(town, dropper.player, type);
		const float after = CallResourceDesire(town, type);
		// 0x404FE9 fsubr: before - after; the alignment of the town's owner (edi = the town, 0x404FEF), with -n the
		// amount asked for (literal `neg`, not verified in game: a huge unsigned n wraps to a positive int, as there)
		effects::alignment::UpdateForResource(TownOwner(town), abode, -static_cast<int32_t>(amount), before - after);
	}
	return removed;
}

uint32_t DoResourceAdding(entt::entity abode, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                          const std::function<uint32_t()>& justAdd)
{
	const auto town = abode_villagers::TownOf(abode);
	// 0x404DFB..0x404E09: no interface or no town -> JustAddResource only (0x404EE3)
	if (!dropper.hasInterface || town == entt::null)
	{
		return justAdd();
	}
	// 0x404E1E..0x404E4D: the desire before and after JustAddResource (vt +0x8C)
	const float before = CallResourceDesire(town, type);
	const uint32_t added = justAdd();
	float delta = before - CallResourceDesire(town, type);
	// 0x404E5B..0x404E70: x Town::GetGameTurnResourceLastRemovedModifier(the interface's player, type)
	delta *= town_stores::GetGameTurnResourceLastRemovedModifier(town, dropper.player, type);
	// 0x404E87: the interface's player's GAlignment::Update(this, type, n, delta), n the argument: an abode's is the
	// amount asked for, which JustAddResource returns whole; a pit's is what its piles took (0x7331A4), justAdd's value
	// here. The type only feeds the alignment history, not kept (UpdateForResource has no type)
	static_cast<void>(amount);
	effects::alignment::UpdateForResource(dropper.player, abode, static_cast<int32_t>(added), delta);
	// 0x404E8C..0x404EC3: AddToBelief(P, delta x (P != the town's owner ? +0xFC : 1) x +0xF8, this, 1, GUIDANCE 1)
	const auto& info = Locator::infoConstants::value().town;
	const float nonOwner = dropper.player != TownOwner(town) ? info.multiplierForNonOwnerAddingResource : 1.0f;
	town_stores::AddToBelief(town, dropper.player, delta * nonOwner * info.multiplierForAddingResourceToTown, abode, true, 1);
	// 0x404ED2: DoCreatureMimicAfterAddingResource (vt +0x68C: StoragePit 0x733810, MultiMapFixed 0x52F210), its result
	// ignored. TODO(creature): ConsiderMakingCreatureMimicPlayer 0x4EA900 is not ported
	return added;
}

uint32_t RemoveResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// StoragePit::RemoveResource 0x7332A0 (vt +0xA0 of the storage pit): the store's piles
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::RemoveResource(object, type, amount, dropper);
	}
	if (auto* a = registry.TryGet<Abode>(object); a != nullptr)
	{
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// Abode::RemoveResource 0x404F10: +0x74 (the building site) with WOOD or -2 -> the site's RemoveResource.
		// TODO(V6): openblack's abodes have no building site; then DoResourceRemoving 0x404F60 (vt +0x8E8)
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceRemoving(object, type, amount, dropper, [&held, amount]() {
			// JustRemoveResource 0x404D60: min(amount, +0xBC[type]) off
			const uint32_t removed = std::min(amount, held);
			held -= removed;
			return removed;
		});
	}
	auto* pot = PotComponent(object);
	if (pot == nullptr)
	{
		return 0;
	}
	const auto owner = StoragePitStore::OwnerOf(object);
	if (owner == entt::null)
	{
		// Pot::RemoveResource 0x66D3F0 / PotStructure::RemoveResource 0x66EE10 without a structure: JustRemoveResource
		// (0x66EEAB), whatever the type (no test)
		return JustRemoveFromPot(object, amount);
	}
	// PotStructure::RemoveResource 0x66EE10, a pile of a storage pit. The building-site branch 0x66EE1E..0x66EE4F is
	// TODO(V6). 0x66EE71..0x66EE9A: over = CalulateAmountOverMaximum (vt +0x8EC); the touched pile gives n - min(over, n)
	// when over > 0, else n
	const int32_t over = StoragePitStore::AmountOverMaximum(owner, type);
	const uint32_t mine = over > 0 ? amount - std::min(static_cast<uint32_t>(over), amount) : amount;
	uint32_t removed = 0;
	// 0x66EE9C..0x66EED3: r = JustRemoveResource(mine) (vt +0x90), then r != 0 -> the pit's DoResourceRemoving(r). The
	// desire before reads the pit's mirror before the pile changes: openblack's mirror is the piles' total, so it runs
	// around the pile's change (the same values)
	if (mine != 0)
	{
		if (pot->amount != 0)
		{
			// DoResourceRemoving's n is r, what the pile gave (0x66EECF)
			const uint32_t r = std::min<uint32_t>(mine, pot->amount);
			removed = DoResourceRemoving(owner, type, r, dropper, [object, mine, owner]() {
				const uint32_t n = JustRemoveFromPot(object, mine);
				StoragePitStore::SyncTotals(owner);
				return n;
			});
		}
		else
		{
			// an empty pile is still asked (0x66EEAB: poison, reaction and fire cleared, SetSize); r == 0 -> no
			// DoResourceRemoving (0x66EEBB)
			JustRemoveFromPot(object, mine);
		}
	}
	// 0x66EED9..0x66EEF5: r < n -> r += the pit's RemoveResource(n - r) (pile 5 -> 1, the touched pile again)
	if (removed < amount)
	{
		removed += StoragePitStore::RemoveResource(owner, type, amount - removed, dropper);
	}
	return removed;
}

uint32_t AddResource(entt::entity object, ResourceType type, uint32_t amount, const pot_resource::Dropper& dropper,
                     bool poisoned)
{
	auto& registry = Entities();
	if (object == entt::null || !registry.Valid(object))
	{
		return 0;
	}
	// StoragePit::AddResource 0x732F60 (vt +0x9C of the storage pit, which is also an Abode)
	if (registry.AllOf<StoragePit>(object))
	{
		return StoragePitStore::AddResource(object, type, amount, dropper, poisoned);
	}
	if (auto* a = AbodeComponent(object); a != nullptr)
	{
		if (type != ResourceType::Food && type != ResourceType::Wood)
		{
			return 0;
		}
		// Abode::AddResource 0x404D90: the +0x74 building-site branch (WOOD or -2) is TODO(V6); else DoResourceAdding
		// 0x404DF0 -> JustAddResource 0x404D40: +0xBC[type] += amount (poisoned is not kept by an abode)
		auto& held = type == ResourceType::Food ? a->foodAmount : a->woodAmount;
		return DoResourceAdding(object, type, amount, dropper, [&held, amount]() {
			held += amount;
			return amount;
		});
	}
	if (registry.AllOf<Pot>(object))
	{
		// PotStructure::AddResource 0x66ED70 / Pot::AddResource 0x66D290 (Mano's pot_resource). (pending) the dropper does
		// not reach a pile of a storage pit through it yet
		return pot_resource::PotStructureAddResource(object, type, amount, poisoned);
	}
	// TODO(V5, Personas): a villager, Villager::AddResource 0x7564D0 (villager::AddResourceToVillager) once it exists
	return 0;
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
