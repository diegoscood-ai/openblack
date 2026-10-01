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

// The villager's decision (state 163 DECIDE_WHAT_TO_DO, Villager::DecideWhatToDo 0x7515C0) and its idle states (209
// NOTHING_TO_DO, 245 GO_AND_CHILLOUT_OUTSIDE_HOME, 246 SIT_AND_CHILLOUT, 252 GO_AND_CHILLOUT_IN_TOWN), as runblack.exe
// W120 does them (docs/bw1-notes/villagers.md; spec dev\tmp_dis\aldeanos\V2_spec.md, disassembly dev\_scratch\mapa\
// v2_a.txt .. v2_f.txt and dev\tmp_dis\aldeanos\core\d_decide*.txt, d_desires.txt). The checks of later milestones are
// neutral (they answer 0 / "no"), never approximated. Positions are MapCoords x / z (ecs::town_queries).

namespace openblack::ecs::villager
{
// ---- the state functions (LivingActionSystem.cpp k_VillagerStateTable) ------------------------------------------

/// 163: Villager::DecideWhatToDo 0x7515C0 (vt +0x8C8). Always 1
uint32_t DecideWhatToDo(components::LivingAction& action);
/// 209: Villager::NothingToDo 0x760000 (`mov eax, 1; ret`)
uint32_t NothingToDo(components::LivingAction& action);
/// 245: Villager::GoAndChilloutOutsideHome 0x76B3F0
uint32_t GoAndChilloutOutsideHome(components::LivingAction& action);
/// 246: Villager::SitAndChillout 0x76B4E0
uint32_t SitAndChillout(components::LivingAction& action);
/// 246's entry: Villager::EnterSitAndChillOut 0x76B570: the counter (+0x58) = initialChillOutTime (+0x394); 1
uint32_t EnterSitAndChillOut(components::LivingAction& action, VillagerStates final, VillagerStates next);
/// 252: Villager::GoAndChilloutInTown 0x76B590 (only a script sets 252)
uint32_t GoAndChilloutInTown(components::LivingAction& action);
/// 114: Villager::ChildFollowsMother 0x7578C0 (VillagerChild.cpp): the child's checks, then a walk (FINAL 114) to a
/// point 5 m from its mother, or from its abode
uint32_t ChildFollowsMother(components::LivingAction& action);

// ---- DecideWhatToDo's checks ---------------------------------------------------------------------------------------

/// Villager::CheckNeededForSomething 0x75FF80: homeless -> CheckHomelessMoveIntoAbode; then CheckNeededForSpecial == 1
uint32_t CheckNeededForSomething(entt::entity villager);
/// Villager::CheckHomelessMoveIntoAbode 0x761360. TODO(V4): neutral 0 (FindAbodeWithSpaceInTown, the homeless list
/// +0x768, AddVillagerToAbode 0x404060 and SetTopState(36))
uint32_t CheckHomelessMoveIntoAbode(entt::entity villager);
/// Villager::CheckNeededForSpecial 0x760010: worship (Milagros' CheckNeededForWorship 0x76BA60), civic, own desires
uint32_t CheckNeededForSpecial(entt::entity villager);
/// Villager::CheckNeededForCivic 0x758180: with a town, fn_7581A0 == 1
uint32_t CheckNeededForCivic(entt::entity villager);
/// fn_7581A0 (also Villager::CheckNeededForTownDesire 0x757C80, a jmp): GetOwnDesiresTrigger, the town desire's
/// CheckVillagerNeededForTownDesire 0x745FF0 (TODO(V3): neutral, its eax taken as 0) and flags &= ~1 (0x7581CF)
uint32_t CheckNeededForTownDesire(entt::entity villager);
/// Villager::GetOwnDesiresTrigger 0x7581E0
[[nodiscard]] float GetOwnDesiresTrigger(entt::entity villager);
/// Villager::GetDesireForLife 0x75BBA0 = GetLifeDesireFromLife(GetLife())
[[nodiscard]] float GetDesireForLife(entt::entity villager);
/// Villager::GetLifeDesireFromLife 0x75BBC0: 1 - ((life - min(D, life)) / (1 - D))^2, D = damageThresholdToGoHome
[[nodiscard]] float GetLifeDesireFromLife(entt::entity villager, float life);
/// Villager::CheckSatisfyOwnDesire 0x760050: the larger of food and life desire (minus the trigger) served first
uint32_t CheckSatisfyOwnDesire(entt::entity villager, float trigger);
/// Villager::CheckSatisfyOwnFoodDesire 0x75BF00: IsHungry ? ChangeStateToFindFoodToEat : 0
uint32_t CheckSatisfyOwnFoodDesire(entt::entity villager);
/// Villager::ChangeStateToFindFoodToEat 0x75B990. TODO(V4): neutral 0
uint32_t ChangeStateToFindFoodToEat(entt::entity villager);
/// Villager::CheckSatisfySleep 0x761490
uint32_t CheckSatisfySleep(entt::entity villager);
/// Villager::CheckWhenGoingToBed 0x760B60. TODO(V4): neutral 0
uint32_t CheckWhenGoingToBed(entt::entity villager);
/// Villager::CheckTakeResourcesToStoragePit 0x7516E0: wood (+0xF6) > minWoodToShowGraphic or food (+0xF4) >
/// minFoodToShowGraphic (signed, jg) -> SetTopState(31); 1
uint32_t CheckTakeResourcesToStoragePit(entt::entity villager);
/// Villager::DiscipleDecideWhatToDo 0x751720. TODO(V14): neutral 0
uint32_t DiscipleDecideWhatToDo(entt::entity villager);
/// Villager::ChildDecideWhatToDo 0x757EC0: CheckChild, CheckNeededForTownDesire, ChildGotoCreche, else 114. Always 1
uint32_t ChildDecideWhatToDo(entt::entity villager);
/// Villager::CheckChild 0x757E80
uint32_t CheckChild(entt::entity villager);
/// Villager::IsMotherAlive 0x757F40. TODO(V14): neutral (the mother is left as it is); 1
uint32_t IsMotherAlive(entt::entity villager);
/// Villager::ChildGotoCreche 0x7579F0. TODO(V14): openblack's towns have no creche (+0x744): 0
uint32_t ChildGotoCreche(entt::entity villager);
/// Villager::CheckNeedNewAbode 0x757F90. TODO(V4): neutral 0
uint32_t CheckNeedNewAbode(entt::entity villager);

// ---- the idle branch ---------------------------------------------------------------------------------------------

/// Villager::SetupNothingToDo 0x753B50: GameRand(9) on the jump table 0x753C64 (0, 1, 1, 1, 2, 2, 2, 2, 2). Always 1
uint32_t SetupNothingToDo(entt::entity villager);
/// Villager::GetChillOutPos 0x753C70: around the town's congregation point, on my side (+-22.5 degrees), R to 10 R
/// away (R = 0.1 x GTownInfo +0x140). 0 without a town
bool GetChillOutPos(entt::entity villager, glm::ivec2& out);
/// Villager::GetPosOutsideMyHouse 0x753D50: Abode::GetPosOutside(3, 0.5 R, 0.5 R), R = GTownInfo +0x144. 0 without a
/// town or an abode
bool GetPosOutsideMyHouse(entt::entity villager, glm::ivec2& out);
/// The member function GetMeToMyChillOutPos takes (GetPosOutsideMyHouse or GetChillOutPos)
using ChillOutPosFn = bool (*)(entt::entity villager, glm::ivec2& out);
/// Villager::GetMeToMyChillOutPos 0x76B610 (pmf, A, R, C): far from A (> R) walk to pmf's point; near: sit (246) where
/// it is clear (CheckForClearArea, IsObject, 1.2 x my radius), turning one step to C (LookAtPos(C, 2)); else walk to a
/// clear point nearby (FindClearArea 5, 1) or to pmf's point. Every walk keeps GetFinalState as its final state
void GetMeToMyChillOutPos(entt::entity villager, ChillOutPosFn pmf, glm::ivec2 a, float r, const glm::ivec2* c);

// ---- test hooks --------------------------------------------------------------------------------------------------

/// The tests: CheckNeededForWorship's stand-in (empty: Milagros' villager_worship::CheckNeededForWorship)
void SetWorshipCheckForTests(std::function<bool(entt::entity)> check);
/// OPENBLACK_TEST_VILLAGER_NOTHING and the tests: the next SetupNothingToDo of `villager` takes `r` for its GameRand(9)
/// (the draw is still made, so the order of the draws does not change)
void ForceNextNothingRoll(entt::entity villager, uint32_t r);
/// The tests: a log of the checks DecideWhatToDo calls, in order (empty: none)
void SetDecideLogForTests(std::function<void(const char*)> log);
} // namespace openblack::ecs::villager
