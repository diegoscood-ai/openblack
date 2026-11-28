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
#include <glm/vec3.hpp>

#include "ECS/Systems/Implementations/VillagerDance.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

// The villagers and the worship site: the town's worship percentage sends them to the site's arrive point (59), up to
// maxDancersVisible (20) dance (60) and the rest wait at the site's door (213); the chants they make cost them life
// (ReduceVillagerLifeByChant); a hungry one eats from the site's food pot (241) and goes back to the dance (65); when
// fewer are needed the first of the go-home queue leaves (248). The walks to the gate and to the door take the site's
// footpaths (SetupMoveToOnFootpath), the walk to the dance place is a plain one, each with its state to follow. The
// dancers dance the site's own dance (its citadel dance, ecs::dances), in its groups' shapes and moves.

namespace openblack::ecs::villager_worship
{
/// The idle check (DECIDE_WHAT_TO_DO): already at the site -> start again; else, if the town worships and
/// GetWorshipersNeeded(1, 1) > 0 -> CheckWorshipActivity. 1 when it went.
bool CheckNeededForWorship(entt::entity villager);
/// A worship site, the town centre functional and built, the site's player is the town's; a villager that cannot get
/// there (CanIGetToTheWorshipSite: within maxDistanceThatVillagersWillGoToWorship, 500) only goes when not
/// `requireReachable` -> GotoWorshipSiteForWorship. A site farther than that is still reachable through the player's
/// teleport stones (TeleportSystem RouteStoneFor): then the villager also starts reacting to the stone it must walk to,
/// so the walk to the site goes through two stones
bool CheckWorshipActivity(entt::entity villager, bool requireReachable);
/// IsVillagerAvailable (not controlled by a script, not in the hand, and its final state's availability bit, the state
/// table's field0xa8 & 1), not a disciple on the first pass (a disciple goes last), and not IsAtOrOnTheWayToWorshipSite
[[nodiscard]] bool IsAvailableForWorshipSite(entt::entity villager, bool secondPass);
/// Flagged at the site, or its final state (during a teleport reaction, the state it goes back to) is 59 / 46
[[nodiscard]] bool IsAtOrOnTheWayToWorshipSite(entt::entity villager);
/// SetState(163 DECIDE_WHAT_TO_DO), as the town's worshipper adjustment sends them back
void SendBackToTown(entt::entity villager);
/// Where the villagers beyond maxDancersVisible worship hidden: the site's door (abode_queries::GetDoorPos: its mesh's
/// door point, else its position), in metres at height 0
[[nodiscard]] glm::vec3 HidingPlace(entt::entity site);
/// A hiding worshipper is there when the site's door is nearer than its speed and one more map unit, measured on the
/// map's whole units; never without a walk
[[nodiscard]] bool IsAtHidingPlace(entt::entity villager, entt::entity site);
/// Hungry (its belly strictly below hungryForFood) with a meal's worth (GetAmountOfFoodRequiredForMeal) in its site's
/// food pot: when `moveToFood`, the walk to the pot's nearest edge, then 241. Every turn at worship asks it; the altar
/// rest (62, 63) asks it without the walk
[[nodiscard]] bool CheckAllowedToRestAtWorshipSite(entt::entity villager, bool moveToFood);
/// At the worship site (counted there: the villager's own at-site flag)
[[nodiscard]] bool IsAtWorshipSite(entt::entity villager);
/// Off the town's worshipper count when at the site, off the site's count and out of its dance, the at-site flag
/// cleared. Sets no state. Called by the worship states and when the town removes the villager
void RemoveVillagerFromWorshipSite(entt::entity villager);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t GotoWorshipSiteForWorshipState(components::LivingAction& action);         ///< 58 (the walk, resumed)
uint32_t ArrivesAtWorshipSiteForWorship(components::LivingAction& action);         ///< 59 (the walk's end)
/// 60: the worship's turn (ProcessInWorship), then, while it stays, its turn in the dance (villager_dance::PerformDance,
/// with `walk` the step of the walk back to its place)
uint32_t WorshippingAtWorshipSite(components::LivingAction& action, const villager_dance::WalkStep& walk);
uint32_t HidingAtWorshipSite(components::LivingAction& action);                    ///< 213
/// 241: at the pot's nearest edge a meal from the pot, eaten, then 65 after the clip; else the walk there
uint32_t GetFoodAtWorshipSite(components::LivingAction& action);
/// 65: back into the dance (StartWorshippingAtWorshipSite), else 163
uint32_t RestartWorshippingAtWorshipSite(components::LivingAction& action);

// The altar rest and the supply errands, as the original has them; nothing in the game sends a villager to the altar,
// and the errands start only from the town's supply desire, which is always 0 (plus a script's boost)
/// 61: along the site's footpath to its rest point (special point 7), then 62; no site -> 163
uint32_t GotoAltarForRest(components::LivingAction& action);
/// 62: still allowed to rest (CheckAllowedToRestAtWorshipSite, without the walk) -> 63 after the clip; else 64's
uint32_t ArrivesAtAltarForRest(components::LivingAction& action);
/// 63: as 62
uint32_t AtAltarRest(components::LivingAction& action);
/// 64: back into the dance, else DECIDE_WHAT_TO_DO's function
uint32_t AtAltarFinishedRest(components::LivingAction& action);
/// The town desire's satisfier for the supplies, and 42: with its storage pit functional, a site and food the site
/// wants that its town can spare: carrying more than that, or all it can -> 44; the pit has food -> along the pit's
/// footpath to its food, then 43; 1 when it set off
uint32_t GotoStoragePitForWorshipSupplies(entt::entity villager);
uint32_t GotoStoragePitForWorshipSuppliesState(components::LivingAction& action); ///< 42
/// 43: carrying enough -> 44; else as much of what is wanted as it can carry from the pit (then 163); else
/// DECIDE_WHAT_TO_DO's function
uint32_t ArrivesAtStoragePitForWorshipSupplies(components::LivingAction& action);
/// 44: 0, as the game has it: nobody sets off with the supplies
uint32_t GotoWorshipSiteWithSupplies(entt::entity villager);
uint32_t GotoWorshipSiteWithSuppliesState(components::LivingAction& action); ///< 44
/// 45: 1, nothing done
uint32_t MoveToWorshipSiteWithSupplies(components::LivingAction& action);
/// 46: all it carries into the site's pot (AtStructureAddResource) and off the site's food on the way; still walking
/// to the pot -> 1; else 163
uint32_t ArrivesAtWorshipSiteWithSupplies(components::LivingAction& action);
bool ExitMoveToWorshipSite(components::LivingAction& action, VillagerStates next); ///< 58, 59
bool ExitAtWorshipSite(components::LivingAction& action, VillagerStates next);     ///< 60, 213
/// 65, 241: unless the next state has the same exit or keeps the villager at worship, RemoveVillagerFromWorshipSite
bool ExitGetFoodAtWorship(components::LivingAction& action, VillagerStates next);
/// 42-46: unless the next state has the same exit, what it carries comes off the site's food on the way
bool ExitMoveToWorshipSiteWithSupplies(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager_worship
