/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownStats.h"

#include <algorithm>
#include <vector>

#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Villager.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Villager/VillagerCore.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

// TownStats.cpp of runblack.exe W120 (TownStats.h)

namespace openblack::ecs::town_stats
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// TownStats::Add(Villager) 0x7492E0
void AddVillager(TownStats& stats, entt::entity villager, const Villager& v)
{
	// 0x7492EF IsChild (vt +0xAF8): children +0x0C (and +0x3C); else adults +0x08
	if (villager::IsChild(villager))
	{
		++stats.children;
	}
	else
	{
		++stats.adults;
	}
	// 0x74930C..0x74931E: +0x54[info +0x1F8 sex]++ (the men +0x54 / the women +0x58, children too; V4's shuffle reads them)
	const auto& info = villager::InfoOf(villager);
	if (info.sex == SexType::Female)
	{
		++stats.females;
	}
	else if (info.sex == SexType::Male)
	{
		++stats.males;
	}
	// 0x749320..0x749341: +0xE4 += (u32) info +0x2D8 foodReqiredForDinner (fild qword: unsigned)
	stats.foodForDinner = static_cast<float>(static_cast<double>(stats.foodForDinner) +
	                                         static_cast<double>(info.foodReqiredForDinner));
	// 0x749347..0x749377: +0xF8 / +0xFC += (movsx) +0xF4 / +0xF6, the food and wood it carries
	stats.foodCarried = stats.foodCarried + static_cast<float>(static_cast<int32_t>(v.resourceHeld.at(0)));
	stats.woodCarried = stats.woodCarried + static_cast<float>(static_cast<int32_t>(v.resourceHeld.at(1)));
	// 0x74937D..0x7493A4: a disciple (flags & 0x200): NumDisciples +0xC8[+0xF2]++ (a byte)
	if ((v.flags & Villager::k_FlagDisciple) != 0 && v.discipleType < stats.disciples.size())
	{
		auto& count = stats.disciples.at(v.discipleType);
		count = static_cast<uint8_t>(count + 1);
	}
}

/// TownStats::Add(Abode) 0x7498C0
void AddAbode(TownStats& stats, const Abode& abode, const GAbodeInfo* info)
{
	// (openblack) an abode without an info record counts with no places and no type
	const uint32_t maxVillagers = info != nullptr ? info->maxVillagersInAbode : 0;   // info +0x174
	const uint32_t maxChildren = info != nullptr ? info->maxChildrenInAbode : 0;     // info +0x178
	// 0x7498C3..0x749916: +0x4C += max villagers, +0x50 += max children, +0x30 += both, +0x44++ (+0x50 / +0x44 not
	// kept: no reader); dynamic_cast<Wonder*> -> +0x48++ (not kept)
	stats.totalPlaces += maxVillagers + maxChildren;
	// +0x4C, the free adult places GetDesireToBeBuilt 0x73A1A0 reads: the original adds MaxVillagers here and moves it
	// in MoveIntoAbode / MoveOutOfAbode / ChildToAdult 0x749490; recomputed as max villagers - the adults housed
	// (+0xB4). (not verified) that it equals that book-keeping (the AbodeVillagers owner to confirm)
	stats.freeAdultPlaces += static_cast<int32_t>(maxVillagers) - static_cast<int32_t>(abode.adultCount);
	// 0x749928..0x74995F: with places: +0x10++, +0x34 += max villagers, +0x40 += max children
	if (maxVillagers + maxChildren != 0)
	{
		++stats.abodesWithPlaces;
		stats.adultPlaces += maxVillagers;
		stats.childPlaces += maxChildren;
	}
	// 0x749962: IsCivic (vt +0x8C0) -> +0x1C++
	if (info != nullptr && IsCivic(info->abodeType))
	{
		++stats.civicBuildings;
	}
	// 0x749973..0x749988: +0x108[info +0x124 abode number]++ (a byte)
	const auto number = info != nullptr ? info->abodeNumber : abode.type;
	if (const auto n = static_cast<size_t>(static_cast<int32_t>(number)); n < stats.abodesByNumber.size())
	{
		auto& count = stats.abodesByNumber.at(n);
		count = static_cast<uint8_t>(count + 1);
	}
}
} // namespace

bool IsCivic(AbodeType type)
{
	switch (static_cast<uint32_t>(type))
	{
	case 0x14:   // Totem (the jump table 0x40604C for 0x14..0x84)
	case 0x24:   // StoragePit
	case 0x44:   // Creche
	case 0x84:   // Workshop
	case 0x100:  // Wonder (0x405FFE je)
	case 0x204:  // Graveyard (0x406027)
	case 0x404:  // TownCentre (0x40602E)
	case 0x1004: // FootballPitch (0x406020)
	case 0x2004: // SpellDispenser (0x406038)
		return true;
	default:
		return false;
	}
}

const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number)
{
	// 0x405B3A..0x405B65: _AbodeInfos 0xC3C690, stride 0x1C8, tribe at +0x158 of the record (ecx starts at +0x158),
	// number at +0x124 ([ecx - 0x34]): the first match
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if ((info.tribeType == tribe || info.tribeType == Tribe::NONE) && info.abodeNumber == number)
		{
			return &info;
		}
	}
	return nullptr; // 0x405B67
}

const GAbodeInfo* AbodeInfoOf(entt::entity abode, Tribe tribe)
{
	auto& registry = Entities();
	const auto* component = registry.TryGet<const Abode>(abode);
	if (component == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(abode);
	const auto meshId = mesh != nullptr ? mesh->id : 0;
	const GAbodeInfo* byTribe = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != component->type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == meshId)
		{
			return &info;
		}
		if (byTribe == nullptr && info.tribeType == tribe)
		{
			byTribe = &info;
		}
	}
	return byTribe;
}

std::vector<entt::entity> AbodesOf(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> abodes;
	const auto* t = registry.TryGet<const Town>(town);
	if (t == nullptr)
	{
		return abodes;
	}
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == t->id)
		{
			abodes.push_back(entity);
		}
	});
	std::sort(abodes.begin(), abodes.end(),
	          [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return abodes;
}

TownStats Compute(entt::entity town)
{
	TownStats stats {};
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return stats;
	}
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto townTribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) as GatherInputs
	registry.Each<const Villager>([&](entt::entity entity, const Villager& v) {
		if (v.town == town)
		{
			AddVillager(stats, entity, v);
		}
	});
	for (const auto abode : AbodesOf(town))
	{
		// Add(Abode) runs once, at MakeFunctional (0x404818..0x40483C, +0x7C bit 1): an abode under construction is out
		const auto& component = registry.Get<Abode>(abode);
		if (!component.addedToTownStats)
		{
			continue;
		}
		AddAbode(stats, component, AbodeInfoOf(abode, townTribe));
	}
	// +0x24 the civic plans: Add(PlannedMultiMapFixed) 0x749A60 / Remove fn_749B10 when the plan's IsCivic (vt +0x50C)
	for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
	{
		if (plans::IsCivic(town, i))
		{
			++stats.civicPlans;
		}
	}
	// +0x100 the wood at the sites: Add(site) 0x749AA0 / Remove fn_749B50 (GetWoodForStats vt +0x104) when the site's
	// GetTown is this town, and the sites' AddResource 0x43C490 / RemoveResource 0x43C530 (the same piles).
	// (approximate) summed in list order, the original in the order of the changes
	for (const auto site : building_sites::SitesOf(town))
	{
		if (building_sites::GetTown(site) == town)
		{
			stats.woodAtSites = stats.woodAtSites + static_cast<float>(building_sites::GetWoodForStats(site));
		}
	}
	return stats;
}
} // namespace openblack::ecs::town_stats
