/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownStats.h"

#include <charconv>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <entt/core/hashed_string.hpp>

#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

// The town's statistics (TownStats.h)

namespace openblack::ecs::town_stats
{
using namespace components;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The buildings with one more counted abode of the town, from its info record (BuildingsOf and Compute alike)
TownBuildings AddCountedAbode(TownBuildings buildings, const Abode& abode, const GAbodeInfo* info)
{
	// (openblack) an abode without an info record counts with no places and no type, by its own abode number
	return AddAbodeBuildings(buildings, info != nullptr ? info->maxVillagersInAbode : 0,
	                         info != nullptr ? info->maxChildrenInAbode : 0, info != nullptr && IsCivic(info->abodeType),
	                         info != nullptr ? info->abodeNumber : abode.type);
}

/// Counts one abode of the town
void AddAbode(TownStats& stats, const Abode& abode, const GAbodeInfo* info)
{
	// (openblack) an abode without an info record counts with no places and no type
	const uint32_t maxVillagers = info != nullptr ? info->maxVillagersInAbode : 0;
	const uint32_t maxChildren = info != nullptr ? info->maxChildrenInAbode : 0;
	// The total space left: both less everyone housed in it, RoomOf's sum over the same abodes; the free child places,
	// the abode count and the wonder count are not kept (no reader)
	stats.totalSpaceLeft =
	    AddAbodeSpaceLeft(stats.totalSpaceLeft, maxVillagers, maxChildren, abode.adultCount, abode.childCount);
	// The room (the adult and child places, the free adult places): RoomOf's sums, the same abodes
	const auto room = AddAbodeRoom(
	    {.adultPlaces = stats.adultPlaces, .childPlaces = stats.childPlaces, .freeAdultPlaces = stats.freeAdultPlaces},
	    maxVillagers, maxChildren, abode.adultCount);
	stats.adultPlaces = room.adultPlaces;
	stats.childPlaces = room.childPlaces;
	stats.freeAdultPlaces = room.freeAdultPlaces;
	// The abodes with places, the civic buildings and the count of its abode number: BuildingsOf's sums, the same abodes
	const auto buildings = AddCountedAbode({.abodesWithPlaces = stats.abodesWithPlaces,
	                                        .civicBuildings = stats.civicBuildings,
	                                        .abodesByNumber = stats.abodesByNumber},
	                                       abode, info);
	stats.abodesWithPlaces = buildings.abodesWithPlaces;
	stats.civicBuildings = buildings.civicBuildings;
	stats.abodesByNumber = buildings.abodesByNumber;
}
} // namespace

bool IsCivic(AbodeType type)
{
	switch (static_cast<uint32_t>(type))
	{
	case 0x14:   // Totem
	case 0x24:   // StoragePit
	case 0x44:   // Creche
	case 0x84:   // Workshop
	case 0x100:  // Wonder
	case 0x204:  // Graveyard
	case 0x404:  // TownCentre
	case 0x1004: // FootballPitch
	case 0x2004: // SpellDispenser
		return true;
	default:
		return false;
	}
}

const GAbodeInfo* FindAbodeInfo(Tribe tribe, AbodeNumber number)
{
	// The abode info records in order: the first match of tribe and number
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if ((info.tribeType == tribe || info.tribeType == Tribe::NONE) && info.abodeNumber == number)
		{
			return &info;
		}
	}
	return nullptr;
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
		if (MeshIdHash(info.meshId) == meshId)
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

entt::id_type MeshIdHash(MeshId mesh)
{
	// the decimal text HashIdentifier formats, hashed over its length
	std::array<char, 16> text {};
	const auto written = std::to_chars(text.data(), text.data() + text.size(), static_cast<uint32_t>(mesh));
	return entt::hashed_string::value(text.data(), static_cast<size_t>(written.ptr - text.data()));
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
	// each abode's creation index read once, then the same comparisons on it (so the same order, ties included)
	std::vector<std::pair<int64_t, entt::entity>> found;
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId == t->id)
		{
			found.emplace_back(object_index::Of(entity), entity);
		}
	});
	std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
	abodes.reserve(found.size());
	for (const auto& [index, entity] : found)
	{
		abodes.push_back(entity);
	}
	return abodes;
}

TownRoom AddAbodeRoom(TownRoom room, uint32_t maxVillagers, uint32_t maxChildren, uint32_t adultsHoused)
{
	// The free adult places: the max villagers less the adults housed (signed: an over-full home takes from the others)
	room.freeAdultPlaces += static_cast<int32_t>(maxVillagers) - static_cast<int32_t>(adultsHoused);
	// With places: its adult and child places
	if (maxVillagers + maxChildren != 0)
	{
		room.adultPlaces += maxVillagers;
		room.childPlaces += maxChildren;
	}
	return room;
}

int32_t AddAbodeSpaceLeft(int32_t spaceLeft, uint32_t maxVillagers, uint32_t maxChildren, uint32_t adultsHoused,
                          uint32_t childrenHoused)
{
	// The original adds both places as the abode is made functional and moves the sum by one as each villager moves into
	// or out of one of the town's homes
	return spaceLeft + static_cast<int32_t>(maxVillagers + maxChildren) - static_cast<int32_t>(adultsHoused) -
	       static_cast<int32_t>(childrenHoused);
}

TownRoom RoomOf(entt::entity town)
{
	TownRoom room {};
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return room;
	}
	const auto id = registry.Get<const Town>(town).id;
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto townTribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) as Compute
	// Every abode of the town once it is made functional (whole sums: the order does not matter)
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId != id || !abode.addedToTownStats)
		{
			return;
		}
		const auto* info = AbodeInfoOf(entity, townTribe);
		const uint32_t maxVillagers = info != nullptr ? info->maxVillagersInAbode : 0;
		const uint32_t maxChildren = info != nullptr ? info->maxChildrenInAbode : 0;
		const auto spaceLeft = AddAbodeSpaceLeft(room.spaceLeft, maxVillagers, maxChildren, abode.adultCount, abode.childCount);
		room = AddAbodeRoom(room, maxVillagers, maxChildren, abode.adultCount);
		room.spaceLeft = spaceLeft;
	});
	return room;
}

TownBuildings AddAbodeBuildings(TownBuildings buildings, uint32_t maxVillagers, uint32_t maxChildren, bool civic,
                                AbodeNumber number)
{
	// With places: one more abode with places
	if (maxVillagers + maxChildren != 0)
	{
		++buildings.abodesWithPlaces;
	}
	// A civic building
	if (civic)
	{
		++buildings.civicBuildings;
	}
	// The count of its abode number (a byte)
	if (const auto n = static_cast<size_t>(static_cast<int32_t>(number)); n < buildings.abodesByNumber.size())
	{
		auto& count = buildings.abodesByNumber.at(n);
		count = static_cast<uint8_t>(count + 1);
	}
	return buildings;
}

TownBuildings BuildingsOf(entt::entity town)
{
	TownBuildings buildings {};
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return buildings;
	}
	const auto id = registry.Get<const Town>(town).id;
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto townTribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) as Compute
	// Every abode of the town once it is made functional, as RoomOf (whole counts: the order does not matter)
	registry.Each<const Abode>([&](entt::entity entity, const Abode& abode) {
		if (abode.townId != id || !abode.addedToTownStats)
		{
			return;
		}
		buildings = AddCountedAbode(buildings, abode, AbodeInfoOf(entity, townTribe));
	});
	return buildings;
}

TownStats AsTheyStand(TownStats stats, entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return stats;
	}
	const auto buildings = BuildingsOf(town);
	stats.abodesWithPlaces = buildings.abodesWithPlaces;
	stats.civicBuildings = buildings.civicBuildings;
	stats.abodesByNumber = buildings.abodesByNumber;
	const auto room = RoomOf(town);
	stats.adultPlaces = room.adultPlaces;
	stats.childPlaces = room.childPlaces;
	stats.freeAdultPlaces = room.freeAdultPlaces;
	stats.totalSpaceLeft = room.spaceLeft;
	stats.woodAtSites = WoodAtSites(town);
	return stats;
}

TownStats Compute(entt::entity town)
{
	TownStats stats {};
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return stats;
	}
	// The villagers' counts as they stand: kept running as villagers come and go, never counted again
	stats = VillagerCounts(registry.Get<const Town>(town).stats);
	const auto* tribe = registry.TryGet<const Tribe>(town);
	const auto townTribe = tribe != nullptr ? *tribe : Tribe::CELTIC; // (openblack, guard) as GatherInputs
	for (const auto abode : AbodesOf(town))
	{
		// An abode is counted once it is made functional: an abode under construction is out
		const auto& component = registry.Get<Abode>(abode);
		if (!component.addedToTownStats)
		{
			continue;
		}
		AddAbode(stats, component, AbodeInfoOf(abode, townTribe));
	}
	// The civic plans: each plan that is civic
	for (plans::PlanIndex i = 0; i < plans::PlansOf(town); ++i)
	{
		if (plans::IsCivic(town, i))
		{
			++stats.civicPlans;
		}
	}
	// The wood at the sites whose town is this town
	stats.woodAtSites = WoodAtSites(town);
	return stats;
}

TownStats VillagerCounts(const TownStats& stats)
{
	return {.adults = stats.adults,
	        .children = stats.children,
	        .males = stats.males,
	        .females = stats.females,
	        .disciples = stats.disciples,
	        .foodForDinner = stats.foodForDinner,
	        .foodCarried = stats.foodCarried,
	        .woodCarried = stats.woodCarried};
}

float AddFoodForDinnerChange(float total, uint32_t oldFood, uint32_t newFood)
{
	// the difference of the two foods as floats, then added to the total
	const float change = static_cast<float>(newFood) - static_cast<float>(oldFood);
	return change + total;
}

float WoodAtSites(entt::entity town)
{
	// The wood at the sites (GetWoodForStats) whose town is this town; the original also follows the sites' resources
	// as they change (the same piles). (approximate) summed in list order, the original in the order of the changes
	float wood = 0.0f;
	for (const auto site : building_sites::SitesOf(town))
	{
		if (building_sites::GetTown(site) == town)
		{
			wood = wood + static_cast<float>(building_sites::GetWoodForStats(site));
		}
	}
	return wood;
}
} // namespace openblack::ecs::town_stats
