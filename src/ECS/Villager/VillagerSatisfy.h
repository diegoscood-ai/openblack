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

// The villager's CheckSatisfy functions of the town desire table: what the town desire calls on a villager when a
// desire is high enough. 1 = the villager took the job. The jobs not ported yet are neutral (0, with their
// TODO), never approximated. CheckSatisfySleep stays in VillagerDecide.

namespace openblack::ecs::components
{
struct LivingAction;
}

namespace openblack::ecs::villager
{
/// Desire 0: the best of the fish farm (FindBestFishFarm) / field (FindBestField) / flock (FindFlockWithoutShepherd)
/// jobs against taking the food carried to the drop-off point (GotoStoragePitForDropOff); the head's
/// VillagerBecomesFisherman / Farmer / Shepherd (TODO: 0)
uint32_t CheckSatisfyFoodDesire(entt::entity villager);
/// The town's flocks without a shepherd, the strictly best GetDistanceModifier(d, 300) above 0, and its score.
/// (pending) the shepherd: none is found until it is ported
[[nodiscard]] entt::entity FindFlockWithoutShepherd(entt::entity town, entt::entity villager, float& score);
/// TODO: 0
uint32_t VillagerBecomesShepherd(entt::entity villager, entt::entity flock);
/// Desire 1 (VillagerForester.cpp): DecideHowToGetWood: 1 -> GotoStoragePitForDropOff; 2 ->
/// SetupMoveToOnFootpath(the BigForest, its arrive point, 53), 1; 3 -> VillagerGotoForest(forest, 49); else 0
uint32_t CheckSatisfyWoodDesire(entt::entity villager);
/// Desire 2: literal 0
uint32_t CheckSatisfyPlaytimeDesire(entt::entity villager);
/// Desire 5: CheckNeededForBuilding (any site); else, once a turn per town (Town requestedPlanThisTurn, set before the
/// request), RequestANewAbode and CheckNeededForBuilding again
uint32_t CheckSatisfyAbodesDesire(entt::entity villager);
/// Desire 6: the same with RequestBestPlanned
uint32_t CheckSatisfyCivicBuildings(entt::entity villager);
/// Desire 7 -> GotoStoragePitForWorshipSupplies. TODO: 0
uint32_t CheckSatisfySupplyWorship(entt::entity villager);
/// Desire 9: GetBestBuildingSite(includeFull = disciple BUILDER) -> SetupBuildingObject (VillagerBuild.h)
uint32_t CheckSatisfyToBuild(entt::entity villager);
/// Desire 12: GetBestRepairBuildingSite -> SetupBuildingObject (the repair sites come from the building side)
uint32_t CheckSatisfyToRepair(entt::entity villager);
/// Desire 13 (the town's best workshop). TODO(workshops): 0
uint32_t CheckSatisfySupplyWorkshop(entt::entity villager);
/// 211 ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS: no town -> 0. The pit functional: there (its walk's goal) and a best
/// workshop with a wood capacity (int16) -> ArrivesAtStoragePitForResource(WOOD, capacity, 210, 163), 1; not there ->
/// SetupMoveToOnFootpath(pit, arrive, 211), 1. Else 163, 1
uint32_t ArrivesAtStoragePitForWorkshopMaterials(components::LivingAction& action);
/// 210 ARRIVES_AT_WORKSHOP_FOR_DROP_OFF: no town -> 0. The best workshop: there (its arrive point) -> all its wood
/// dropped and given to the workshop (workshops::AddResource), then 163; not there -> SetupMoveToOnFootpath(workshop,
/// arrive, 210), 1. No workshop -> 163, 1
uint32_t ArrivesAtWorkshopForDropOff(components::LivingAction& action);
/// Desire 15: the town sets the villager's activity: the highest activity desire of the football, the player's
/// creature and the artifacts; best 0 -> 0, else that one's activity. TODO(football/creature/artifacts): none of the
/// three is ported, so literally 0
uint32_t CheckSatisfyRelaxation(entt::entity villager);
} // namespace openblack::ecs::villager
