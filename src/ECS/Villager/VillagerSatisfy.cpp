/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSatisfy.h"

#include <string>

#include <entt/entity/entity.hpp>
#include <fmt/format.h>

#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerResources.h"
#include "InfoConstants.h"
#include "Locator.h"

// Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerSatisfy.h)

namespace openblack::ecs::villager
{
using namespace components;

namespace
{
void TraceIf(entt::entity villager, const std::string& line)
{
	if (TraceOn(villager))
	{
		Trace(villager, line);
	}
}

/// The town's three food-job lists CheckSatisfyFoodDesire 0x759F30 searches: the fish farms (Town +0x788, fn_0073E750),
/// the fields (+0x780, Town::FindBestField 0x73E870) and the flocks (+0xF08, fn_0073E7F0). openblack keeps
/// no fish farm / field list in the town: a FishFarm whose town is this one (the FishFarm ctor adds itself to the nearest
/// town's list, 0x52C407), a Field with this town's id (the Field ctor adds itself to its town argument's list,
/// 0x527E64). V5 scope (decided by the session lead, P-1 of V5_spec): literal only when the three lists are empty
bool HasFoodJobCandidates(entt::entity town, const Town& t)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!t.flocks.empty())
	{
		return true;
	}
	bool found = false;
	registry.Each<const FishFarm>([&found, town](const FishFarm& farm) { found = found || farm.town == town; });
	if (found)
	{
		return true;
	}
	registry.Each<const Field>([&found, &t](const Field& field) {
		found = found || static_cast<uint32_t>(field.town) == static_cast<uint32_t>(t.id);
	});
	return found;
}
} // namespace

uint32_t CheckSatisfyFoodDesire(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* v = registry.TryGet<const Villager>(villager);
	// 0x759F3B: GetTown (vt +0x48). (openblack, guard) the desires only reach villagers of a town
	const auto* t = v != nullptr && v->town != entt::null && registry.Valid(v->town) ? registry.TryGet<const Town>(v->town)
	                                                                                 : nullptr;
	if (t == nullptr)
	{
		return 0;
	}
	// 0x759F50..0x75A00A: for k 0..2 a node {score, object, k}: k 0 fn_0073E750 (the fish farms: ftol((1 - min(1,
	// fishermen / 4)) x 1.0) x GetDistanceModifier(d, 500)), k 1 FindBestField 0x73E870 (GetDesireToBeFarmed 0x5293A0
	// x GetDistanceModifier(d, 300)), k 2 fn_0073E7F0 (the flocks without a shepherd: GetDistanceModifier(d, 300)),
	// each the strictly best above 0 or {0, null}; inserted before the first node with a strictly smaller score (high to
	// low, ties keep k's order). TODO(V8/V10): the finders and VillagerBecomesFisherman 0x75B560 / Farmer 0x759C00 /
	// Shepherd 0x768BE0 are not ported: with any candidate in the lists the jobs would decide, so neutral 0
	// ((approximate) a list whose objects all score 0 would let the original go on to the drop-off below)
	if (HasFoodJobCandidates(v->town, *t))
	{
		TraceIf(villager, "food-desire: job TODO(V8) -> 0");
		return 0;
	}
	// Here the three finders give {0, null}: the head is {0, null, k 0}.
	// 0x75A010..0x75A058: frac = (float)(1 - (GetFoodCapacity (0x7514D0) + 1e-5) / (MaxFoodCarried (+0x264) + 1e-5));
	// 0x75A05C..0x75A07C: GetDistanceModifier(GetDistanceInMetres(GetResourceDropoffPos(FOOD), me), 500) x frac
	// (DropOffScore; asking for the drop-off point may make the town's temporary food pot: a side effect, literal)
	const auto& info = InfoOf(villager);
	const auto pos = GetResourceDropoffPos(villager, ResourceType::Food);
	const auto distance = town_queries::GetDistanceInMetres(pos, town_queries::PosOf(villager));
	const float drop = DropOffScore(v->resourceHeld.at(0), info.maxFoodCarried, distance);
	// 0x75A084..0x75A090: drop > head.score (fcomp; `test ah, 0x41; jne`) -> GotoStoragePitForDropOff 0x769620 (its
	// result), after the list is freed (fcomp dword: a float compare)
	constexpr float k_HeadScore = 0.0f;
	if (drop > k_HeadScore)
	{
		TraceIf(villager, fmt::format("food-desire: drop {:.9f} best 0 0 -> 31", drop));
		return GotoStoragePitForDropOff(villager);
	}
	// 0x75A0C1..0x75A0C6: head.object == 0 -> 0x75A1F4: 0
	TraceIf(villager, fmt::format("food-desire: drop {:.9f} best 0 0 -> 0", drop));
	return 0;
}

uint32_t CheckSatisfyWoodDesire([[maybe_unused]] entt::entity villager)
{
	// TODO(V9): 0x75F4A0 (DecideHowToGetWood, VillagerGotoForest, BigForest, GotoStoragePitForDropOff). Neutral
	return 0;
}

uint32_t CheckSatisfyPlaytimeDesire([[maybe_unused]] entt::entity villager)
{
	return 0; // 0x763130: xor eax, eax; ret
}

// CheckSatisfyAbodesDesire 0x758E30 / CheckSatisfyCivicBuildings 0x758E90: VillagerBuild.cpp (V7)

uint32_t CheckSatisfySuppyWorship([[maybe_unused]] entt::entity villager)
{
	// TODO(milagros2): 0x76CC00 -> GotoStoragePitForWorshipSupplies 0x76BFA0. Neutral (the desire 7 is always 0 plus
	// the boosts)
	return 0;
}

// CheckSatisfyToBuild 0x759330 / CheckSatisfyToRepair 0x759370: VillagerBuild.cpp (V7)

uint32_t CheckSatisfySupplyWorkshop([[maybe_unused]] entt::entity villager)
{
	// TODO(talleres): 0x7593A0 (Town::GetBestWorkshop 0x740250). Neutral
	return 0;
}

uint32_t CheckSatisfyRelaxation([[maybe_unused]] entt::entity villager)
{
	// 0x761460 -> Town::SetVillagerActivity 0x73FF10: the best GetVillagerActivityDesire (vt +0x4C) of the football
	// (+0xEA4), the player's creature (+0xA4C) and the town's artifacts (+0x994). TODO(fútbol/criatura/artefactos):
	// openblack has none of them, so the best is 0 and the answer is 0 (literal)
	return 0;
}
} // namespace openblack::ecs::villager
