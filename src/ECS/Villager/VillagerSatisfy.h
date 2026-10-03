/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/fwd.hpp>

// The villager's CheckSatisfy functions of the town desire table (GTownDesireFunction +0x40, runblack.exe W120; spec
// dev\tmp_dis\aldeanos\V3_spec.md §6.2): what TownDesire::CheckVillagerNeededForTownDesire 0x745FF0 calls when a
// desire is high enough. 1 = the villager took the job. The jobs of later milestones are neutral (0, with their
// TODO), never approximated. CheckSatisfySleep 0x761490 (V2) stays in VillagerDecide.

namespace openblack::ecs::villager
{
/// Villager::CheckSatisfyFoodDesire 0x759F30 (desire 0). TODO(V8): the farmers / fishermen; 0
uint32_t CheckSatisfyFoodDesire(entt::entity villager);
/// Villager::CheckSatisfyWoodDesire 0x75F4A0 (1: DecideHowToGetWood, VillagerGotoForest, BigForest,
/// GotoStoragePitForDropOff). TODO(V9): 0
uint32_t CheckSatisfyWoodDesire(entt::entity villager);
/// Villager::CheckSatisfyPlaytimeDesire 0x763130 (2): `xor eax, eax; ret`, literal 0
uint32_t CheckSatisfyPlaytimeDesire(entt::entity villager);
/// Villager::CheckSatisfyAbodesDesire 0x758E30 (5: CheckNeededForBuilding, Town +0x5E4, RequestANewAbode).
/// TODO(V6/V7): 0, without touching Town::requestedPlanThisTurn
uint32_t CheckSatisfyAbodesDesire(entt::entity villager);
/// Villager::CheckSatisfyCivicBuildings 0x758E90 (6: RequestBestPlanned). TODO(V6/V7): 0
uint32_t CheckSatisfyCivicBuildings(entt::entity villager);
/// Villager::CheckSatisfySuppyWorship 0x76CC00 (7) -> GotoStoragePitForWorshipSupplies 0x76BFA0. TODO(milagros2): 0
uint32_t CheckSatisfySuppyWorship(entt::entity villager);
/// Villager::CheckSatisfyToBuild 0x759330 (9). TODO(V7): 0 (with no building sites the original gives 0 too)
uint32_t CheckSatisfyToBuild(entt::entity villager);
/// Villager::CheckSatisfyToRepair 0x759370 (12). TODO(V11): 0
uint32_t CheckSatisfyToRepair(entt::entity villager);
/// Villager::CheckSatisfySupplyWorkshop 0x7593A0 (13: Town::GetBestWorkshop 0x740250). TODO(talleres): 0
uint32_t CheckSatisfySupplyWorkshop(entt::entity villager);
/// Villager::CheckSatisfyRelaxation 0x761460 (15) = Town::SetVillagerActivity 0x73FF10(villager): the highest
/// GetVillagerActivityDesire (vt +0x4C) of the football +0xEA4, the player's creature (+0xA4C) and the artifacts
/// (+0x994, next +0x20); best 0 -> 0, else its vt +0x50. TODO(fútbol/criatura/artefactos): none of the three is
/// ported, so literally 0
uint32_t CheckSatisfyRelaxation(entt::entity villager);
} // namespace openblack::ecs::villager
