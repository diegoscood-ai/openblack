/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerSatisfy.h"

#include <entt/entity/entity.hpp>

// Villager.cpp / VillagerStates.cpp of runblack.exe W120 (VillagerSatisfy.h)

namespace openblack::ecs::villager
{
uint32_t CheckSatisfyFoodDesire([[maybe_unused]] entt::entity villager)
{
	// TODO(V8): 0x759F30 (the field / fish farm jobs). Neutral
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

uint32_t CheckSatisfyAbodesDesire([[maybe_unused]] entt::entity villager)
{
	// TODO(V6/V7): 0x758E30 (CheckNeededForBuilding, the plan of the turn +0x5E4, RequestANewAbode). Neutral
	return 0;
}

uint32_t CheckSatisfyCivicBuildings([[maybe_unused]] entt::entity villager)
{
	// TODO(V6/V7): 0x758E90 (CheckNeededForBuilding, +0x5E4, RequestBestPlanned). Neutral
	return 0;
}

uint32_t CheckSatisfySuppyWorship([[maybe_unused]] entt::entity villager)
{
	// TODO(milagros2): 0x76CC00 -> GotoStoragePitForWorshipSupplies 0x76BFA0. Neutral (the desire 7 is always 0 plus
	// the boosts)
	return 0;
}

uint32_t CheckSatisfyToBuild([[maybe_unused]] entt::entity villager)
{
	// TODO(V7): 0x759330 (the building sites). Neutral
	return 0;
}

uint32_t CheckSatisfyToRepair([[maybe_unused]] entt::entity villager)
{
	// TODO(V11): 0x759370. Neutral
	return 0;
}

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
