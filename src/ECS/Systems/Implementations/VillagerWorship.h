/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

// The villagers and the worship site (Villager.cpp 0x76BA60..0x76CC00, research sources.md §4.2): the town's worship
// percentage sends them to the site's arrive point (59), up to maxDancersVisible (20) dance (60) and the rest wait at
// the hide point (213); the chants they make cost them life (ReduceVillagerLifeByChant); when fewer are needed the
// first of the go-home queue leaves (248). The movement is openblack's (the WallHug pathfinding): the original walks on
// the footpath to the site (Living::SetupMoveToOnFootpath) and to its dance place (Villager::SetupMoveToPos).

namespace openblack::ecs::villager_worship
{
/// Villager::CheckNeededForWorship 0x76BA60 (the idle check, DECIDE_WHAT_TO_DO): already at the site -> start again;
/// else, if the town worships and GetWorshipersNeeded(1, 1) > 0 -> CheckWorshipActivity. 1 when it went.
bool CheckNeededForWorship(entt::entity villager);
/// Villager::CheckWorshipActivity 0x76BAE0: a worship site, the town centre functional and built, the site's player is
/// the town's; a villager that cannot get there (CanIGetToTheWorshipSite 0x76BC20: within
/// maxDistanceThatVillagersWillGoToWorship, 500) only goes when not `requireReachable` -> GotoWorshipSiteForWorship.
/// A site farther than that is still reachable through the player's teleport stones (GPlayer fn_0064D6B0 =
/// teleport::FindRouteStone): then the villager also starts reacting to the stone it must walk to (0x76BB99..0x76BC07),
/// so the walk to the site goes through two stones
bool CheckWorshipActivity(entt::entity villager, bool requireReachable);
/// Villager::IsAvailableForWorshipSite 0x752820: IsVillagerAvailable (the state table's availability bit
/// field0xa8 & 1), not flagged 0x200 on the first pass, and not IsAtOrOnTheWayToWorshipSite 0x752860
[[nodiscard]] bool IsAvailableForWorshipSite(entt::entity villager, bool secondPass);
/// Villager::IsAtOrOnTheWayToWorshipSite 0x752860: flagged at the site (+0xE0 & 2), or its state 59 / 46
[[nodiscard]] bool IsAtOrOnTheWayToWorshipSite(entt::entity villager);
/// Villager::SetState(163 DECIDE_WHAT_TO_DO), vt 0x8E8, as Town::AdjustWorshipersWorshipping sends them back
void SendBackToTown(entt::entity villager);
/// Villager +0xE0 & 2: at the worship site (components::WorshipVillager::atSite)
[[nodiscard]] bool IsAtWorshipSite(entt::entity villager);
/// Villager::RemoveVillagerFromWorshipSite 0x76C440: the town's worshipper count (fn_0073E3F0) when at the site, off
/// the site's count (0x77D0A0) and out of its dance (vt 0x978 / 0xB08), +0xE0 & 2 cleared. Sets no state. Called by
/// the worship states and by Town::RemoveVillager 0x73E2A7..0x73E2B2
void RemoveVillagerFromWorshipSite(entt::entity villager);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t GotoWorshipSiteForWorshipState(components::LivingAction& action); ///< 58, 0x76BCC0 (the walk, resumed)
uint32_t ArrivesAtWorshipSiteForWorship(components::LivingAction& action); ///< 59, 0x76BE00 (after the walk)
uint32_t WorshippingAtWorshipSite(components::LivingAction& action);       ///< 60, 0x76C680
uint32_t HidingAtWorshipSite(components::LivingAction& action);            ///< 213, 0x76C5E0
bool ExitMoveToWorshipSite(components::LivingAction& action, VillagerStates next); ///< 58, 59: 0x76C170
bool ExitAtWorshipSite(components::LivingAction& action, VillagerStates next);     ///< 60, 213: 0x76C1F0
} // namespace openblack::ecs::villager_worship
