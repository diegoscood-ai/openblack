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

#include <functional>

#include <entt/entity/fwd.hpp>
#include <glm/vec2.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack
{
struct GVillagerInfo;
}

// The villager's home (VillagerHome.cpp of runblack.exe W120; spec dev\tmp_dis\aldeanos\V4_spec.md §3, §4, §6, §7.2,
// disassembly dev\tmp_dis\aldeanos\v4\home.txt, homeless.txt, misc.txt, misc3.txt, misc4.txt, helpers.txt; V2's part in
// V2_spec.md §6.6): going home (36), arriving (37), staying in (38), going to bed (119, 120, 121), the homeless (129), the
// vagrants (130), the tent next to a tree (238), the child that goes home to change (234), the bit 4 of +0xE0 with
// Abode::presentAtHome, and the moves between abodes. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the villager's links ----------------------------------------------------------------------------------------

/// Villager::SetAbode 0x750DE0: +0x128 = abode; SetTown(0); with an abode SetTown(abode.GetTown())
void SetAbode(entt::entity villager, entt::entity abode);
/// Villager::SetTown 0x756530: +0x12C = town
void SetTown(entt::entity villager, entt::entity town);
/// Villager +0xE0 & 4: inside its home (ArriveHome 0x751FAC / LeaveHome 0x751FE0)
[[nodiscard]] bool IsAtHome(entt::entity villager);
/// Villager::IsReachable 0x756460 (vt +0x530): IsAvailable (vt +0x2C) && !IsAtHome && !(+0x24 & 4) && TOP != 236
/// GO_AND_HIDE_IN_NEARBY_BUILDING. +0x24 & 4 = in the hand (PlaceObjectInMagicHand 0x5FB014; fire::traits::InHand)
[[nodiscard]] bool IsReachable(entt::entity villager);
/// Villager::IsVillagerAvailable 0x752290: !(+0x25 & 4) (script), IsAvailableForStateChange (!(+0x24 & 4), not in
/// the hand) and GetVillagerAvailableState 0x751F40 (GetFinalState's row, file 0xA8) & 1
[[nodiscard]] bool IsVillagerAvailable(entt::entity villager);
/// Villager::ArriveHome 0x751FA0: with an abode, flags |= 4 and Abode::ArriveHome (++presentAtHome). The bit is not
/// tested first: two calls count two (literal)
void ArriveHome(entt::entity villager);
/// Villager::LeaveHome 0x751FD0: flags & 4 -> flags &= 0xDFFB (4 and 0x2000) and, with an abode, Abode::LeaveHome
void LeaveHome(entt::entity villager);

// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// Villager::GoHome 0x760270 = DoGoingHome(37 ARRIVES_HOME, 238 SLEEP_IN_TENT). Always 1
uint32_t GoHome(entt::entity villager);
/// State 36 GO_HOME: GoHome
uint32_t GoHomeState(components::LivingAction& action);
/// Villager::DoGoingHome 0x760280 (arrive, tent): with an abode, inside -> 38; already on the way -> nothing; else the
/// walk to its door with FINAL `arrive`. Without one: no town -> 130 VAGRANT_START; more than 100 m from the town -> a
/// walk to 10..35 m from it on my side (FINAL the TOP); else a tent near me (FINAL `tent`) or a stroll (FINAL the TOP).
/// TODO(baile): IsDancing / RemoveFromDance (0x760289). Always 1
uint32_t DoGoingHome(entt::entity villager, VillagerStates arrive, VillagerStates tent);
/// Living::SetupMoveToOnFootpath 0x5EDD20 (object, pos, final): standing on the object's arrive point and going
/// elsewhere -> SetupMoveToWithHug(pos, final); else the object's UseFootpathIfNecessary (vt +0x80)
void SetupMoveToOnFootpath(entt::entity villager, entt::entity object, glm::ivec2 pos, VillagerStates final);
/// Villager::ArrivesHome 0x760930 (37, also 249 and from 35): not at the door -> the walk again (FINAL 37); built and
/// repaired -> in; hurt: a functional abode -> in, else a tent (238); hungry: in (with SetTopState(163) first when it
/// is not functional, literal); else SetupBuildingObject (TODO(V7/V11): 0) and in. No abode -> 129 and 0
uint32_t ArrivesHome(entt::entity villager);
/// State 37 ARRIVES_HOME
uint32_t ArrivesHomeState(components::LivingAction& action);
/// State 38 AT_HOME: Villager::AtHome 0x760B10 = HomeDecideWhatToDo; 1
uint32_t AtHome(components::LivingAction& action);
/// Villager::HomeDecideWhatToDo 0x75FEA0: the emergency -> 119; CheckNeedsAtHome; a disciple that ignores the needs
/// (a BREEDER with Sleep first: CheckSatisfySleep) -> DecideWhatToDo; CheckNeededForSomething; HomeNothingToDo and 0
uint32_t HomeDecideWhatToDo(entt::entity villager);
/// Villager::CheckNeedsAtHome 0x760110: a woman's WomanSpecial / pregnancy; CheckSatisfyOwnDesire(0.9 x max(
/// GetLifeDesireFromLife(D), POWER(F))) with (D, F) = (+0x360, +0x2C0), (+0x35C, +0x2C4) for a disciple that ignores
/// the needs; a child's CheckChildActivity (ChildDecideWhatToDo, always 1)
uint32_t CheckNeedsAtHome(entt::entity villager);
/// Villager::HomeNothingToDo 0x75FFB0: inside and GameRand(4) == 0 (VillagerHome.cpp 0x4B) -> counter 0,
/// SetTopState(119); else SetupNothingToDo. 1
uint32_t HomeNothingToDo(entt::entity villager);
/// Villager::ExitAtHome 0x761B40 (the exit of 35..38, 118..121, 125..127, 248, 249): Infos[next] file 0xC0
/// (StaysAtHomeOnExit) == 0 -> LeaveHome. 1
uint32_t ExitAtHome(components::LivingAction& action, VillagerStates next);
/// State 119 GOTO_BED_AT_HOME: Villager::GotoBedAtHome 0x760B30: SetTopState(120), then the counter = RestAtHomeTime
/// (+0x24C). 1
uint32_t GotoBedAtHome(components::LivingAction& action);
/// State 120 SLEEPING_AT_HOME: Villager::SleepingAtHome 0x760D70: with a town --counter; at 0 DoSleeping(1) == 0 -> 38. 1
uint32_t SleepingAtHome(components::LivingAction& action);
/// Villager::DoSleeping 0x760DB0 (f): poisoned -> 0; life < info.life -> IncreaseLife(f x RestAtHomeRestoresLifeBy);
/// Sleep (16) first in the town's order 1, or life < DamageThresholdToSleepUntil (+0x360) -> counter = RestAtHomeTime,
/// 1; else 0
uint32_t DoSleeping(entt::entity villager, float f);
/// State 121 WAKE_UP_AT_HOME: Villager::WakeUpAtHome 0x760E50 = jmp GoHome (no code sets 121)
uint32_t WakeUpAtHome(components::LivingAction& action);
/// Villager::CheckWhenGoingToBed 0x760B60 (CheckSatisfySleep inside): once a stay (flags 0x2000); the old age's death
/// -> 0; else the pair's CheckGetPregnantAtHome (neutral, P-2) and 1
uint32_t CheckWhenGoingToBed(entt::entity villager);
/// Villager::CheckGetPregnantAtHome 0x760C80 = WillHousewifeGetPregnant(0) 0x7624C0. TODO(V14): neutral (P-2: a
/// pregnancy without its birth, V14, would keep the woman at home for ever); 0
uint32_t CheckGetPregnantAtHome(entt::entity villager);
/// State 238 SLEEP_IN_TENT (also 250): Villager::SleepInTent 0x761AE0
uint32_t SleepInTent(entt::entity villager);
uint32_t SleepInTentState(components::LivingAction& action);
/// State 129 HOMELESS_START: Villager::HomelessStart 0x761320: CheckHungry, CheckNeededForSomething,
/// CheckHomelessMoveIntoAbode, else SetupNothingToDo. 1
uint32_t HomelessStart(components::LivingAction& action);
/// State 130 VAGRANT_START: Villager::VagrantStart 0x76A8D0: a town of my tribe within 200 m that takes me -> 163;
/// hurt -> a tent; else a stroll ahead (FINAL 130). 1
uint32_t VagrantStart(entt::entity villager);
uint32_t VagrantStartState(components::LivingAction& action);
/// State 234 GO_HOME_AND_CHANGE: Villager::GoHomeAndChange 0x761810: to the door (FINAL 234, a direct walk); there ->
/// 37 or 38 (inside); no abode -> 163; then a scale below 0.95 -> SetScaleForAge. 1
uint32_t GoHomeAndChange(components::LivingAction& action);
/// Villager::ExitGoHomeAndChange 0x761980: another exit -> ChangeTribeIfRequired(the town's tribe, else the info's,
/// Infos[next] 0xC0 == 0); CHANGE_HOUSE disciple -> SetVillagerDisciple(0) TODO(V14). 1
uint32_t ExitGoHomeAndChange(components::LivingAction& action, VillagerStates next);

// ---- the abode and the town --------------------------------------------------------------------------------------

/// Villager::CheckHomelessMoveIntoAbode 0x761360: a town and FindAbodeWithSpaceInTown(me, 0) -> out of the homeless
/// list, AddVillagerToAbode, SetTopState(36); 1. Else 0
uint32_t CheckHomelessMoveIntoAbode(entt::entity villager);
/// Villager::MakeHomeless 0x761220: MakeHomelessNoStateChange and SetTopState(129); its result
bool MakeHomeless(entt::entity villager);
/// Villager::MakeHomelessNoStateChange 0x761240: out of its abode (SetAbode(0), SetTown(town)); no town -> 0; already
/// in the list -> 0; out of the vagrants; at the head of the town's homeless list; 1
bool MakeHomelessNoStateChange(entt::entity villager);
/// Villager::HomeDeleted 0x7611F0 (its abode is destroyed): +0x60 == the abode -> 0 (TODO: +0x60 is not identified);
/// an abode -> MakeHomeless; else TownDeleted 0x750B50 (TODO(V12))
void HomeDeleted(entt::entity villager);
/// Villager::CheckNeedNewAbode 0x757F90: a child -> 0; an abode that is not too crowded -> 0; no town -> VagrantStart, 1;
/// a better abode (FindAbodeWithSpaceInTown above the current score) and MoveVillagerToAbode -> 36 if available, 1;
/// else MakeHomeless unless already in the list, 1
uint32_t CheckNeedNewAbode(entt::entity villager);
/// Villager::MoveVillagerToAbode 0x758080: room left (children / adults, signed) > 0 -> ForceMoveVillagerToAbode, 1;
/// else 0
uint32_t MoveVillagerToAbode(entt::entity villager, entt::entity abode);
/// Villager::ForceMoveVillagerToAbode 0x756240: the same town -> AddVillagerToAbode; else the old town's RemoveVillager
/// and, below 100% full (children / adults), AddVillagerToAbode, else the new town's AddVillagerToTown
void ForceMoveVillagerToAbode(entt::entity villager, entt::entity abode);
/// Villager::ChangeTribeIfRequired 0x7618C0 (tribe, leaving): KeepMeshWhenChangeTown (+0x388) == 0 -> ChangeInfo(
/// GVillagerInfo::Find(tribe, my number)) and, leaving, CreateSmokyStuff(1, 1.0, -1) 0x63A810
void ChangeTribeIfRequired(entt::entity villager, Tribe tribe, bool leaving);
/// Villager::ChangeInfo 0x761A00: +0x28 = the info; a child the three meshes = ChildMeshHigh (+0x204), an adult
/// GetDetailMesh(2 / 1 / 0): the only place a grown-up child gets its adult mesh. 1
uint32_t ChangeInfo(entt::entity villager, const GVillagerInfo& info);
/// The villager's mesh (Mesh, or SkeletalAnimation::hiddenMesh while it is not drawn): a child detail_meshes::Villager(
/// info, true) (SetAge's +0x20C / +0x208 / +0x204), or ChildMeshHigh when `childHighOnly` (ChangeInfo's); an adult
/// detail_meshes::Villager(info, false)
void SetVillagerMeshes(entt::entity villager, const GVillagerInfo& info, bool child, bool childHighOnly);
/// GVillagerInfo::Find 0x752650: the FIRST record of that tribe (+0x1F4) and number (+0x1FC); nullptr when none
[[nodiscard]] const GVillagerInfo* FindVillagerInfo(Tribe tribe, VillagerNumber number);
/// Villager::FindPosOutsideAbode 0x753470 (abode; entt::null -> its own): door + GetPosFromAngle(Get3DAngleFromXZ(
/// abode, door) + (pi/8 - GameFloatRand(pi/4)) (Villager.cpp 0xA97), GameFloatRand(1.5) + 1.5 (0xA96))
[[nodiscard]] glm::ivec2 FindPosOutsideAbode(entt::entity villager, entt::entity abode);
/// Villager::SetupBuildingObject 0x758530 (abode): the abode's repair site (AddBuildingSite 0x73B8E0) when it is not
/// built or repaired. TODO(V7/V11): neutral 0 (no repairing on arrival)
uint32_t SetupBuildingObject(entt::entity villager, entt::entity abode);
/// Villager::IsSexuallyActive 0x761090: StartHavingSexAge (+0x228) <= age < StopHavingSexAge (+0x22C)
[[nodiscard]] bool IsSexuallyActive(entt::entity villager);

// ---- the tent ----------------------------------------------------------------------------------------------------

/// Villager::GetTentPos 0x7604F0 (pos&): the nearest tree within 50 m (fn_00604AF0 + IsTree) with room (fn_0074C650)
/// -> 2 m from it; else 3 tries near `pos`: a clear cell (Collide & 0x19 == 0) whose 9 spiral cells (the spiral moves
/// `pos` itself) have no villager in 238 within 5 m -> pos 9 spiral steps away; a failed try moves pos by
/// GetPosFromAngle(GameFloatRand(2 pi), GameFloatRand(5) + 3) (VillagerHome.cpp 0x16D, the 5 first). 0 / 1
bool GetTentPos(entt::entity villager, glm::ivec2& pos);
/// fn_00604AF0 with Villager::FUN_00761BC0 (IsTree, vt +0x338) as the filter: map_cells::FindNearestInSpiral
[[nodiscard]] entt::entity FindNearestTree(glm::ivec2 pos, float radius);
/// fn_0074C650 (tree, villager, &out): the 9 spiral cells from the tree; a villager in 238, or a non-villager with
/// +0x24 & 2 (a MultiMapFixed: map_cells::IsMultiMapFixedClass), within distance - Get2DRadius < 4 of the tree is an
/// occupant; a second one -> 0. out = the tree + 2 m away from the occupant (or towards the villager). 1
bool TentNextToTree(entt::entity tree, entt::entity villager, glm::ivec2& out);

// ---- test hooks --------------------------------------------------------------------------------------------------

/// The tests: the nearest tree search and MapCoords::Collide (empty: map_cells', which need the map)
void SetTentQueriesForTests(std::function<entt::entity(glm::ivec2 pos, float radius)> nearestTree,
                            std::function<uint32_t(glm::ivec2 pos)> collide);
} // namespace openblack::ecs::villager
