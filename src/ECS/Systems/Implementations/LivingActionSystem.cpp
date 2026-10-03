/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "LivingActionSystem.h"

#include <bitset>

#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/LivingPhysics.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateTable.h"
#include "VillagerFire.h"
#include "VillagerReactions.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"
#include "VillagerWorship.h"
#include "ECS/VillagerDrowning.h"
#include "Enums.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

uint32_t VillagerInvalidState(LivingAction& action)
{
	SPDLOG_LOGGER_ERROR(spdlog::get("ai"), "Villager #{}: Stuck in an invalid state",
	                    static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)));
	assert(false);
	return 0;
}

namespace
{
/// MobileWallHug::MoveTo 0x60AF20's result for openblack's walk (PathfindingSystem::Update runs before the state
/// functions, Game.cpp): 0xA (arrived) only from ARRIVED 0x60AFC0 when AreWeThere(0) and from FINAL_STEP 0x60AF6C, both
/// after putting the object on the goal (Pos = GetDestPos, MoveMapObject vt +0x55C: 0x60AF6C..0x60AFB6 and
/// 0x60AFC0..0x60B018). PathfindingSystem sets FINAL_STEP when AreWeThere (its step 5) and puts the villager on the goal
/// on the next turn (ApplyStepGoal<FinalStep>, step 4b; ARRIVED the same): arrived when the tag is there and the villager
/// stands on its goal, as the original returns 0xA one turn after STEP_THROUGH set FINAL_STEP. Anything else is a walk
/// still going (0 / 1 / 6 / 7: Living::MoveToPos does nothing). There is no "abandoned" result (PathfindingSystem's
/// AbandonMove goes on as STEP_THROUGH).
uint32_t WallHugMoveToResult(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr)
	{
		return 0;
	}
	const auto at = glm::xz(transform->position);
	if (const auto* finalStep = registry.TryGet<const MoveStateFinalStepTag>(entity);
	    finalStep != nullptr && finalStep->stepGoal == at)
	{
		return 0xA;
	}
	if (const auto* arrived = registry.TryGet<const MoveStateArrivedTag>(entity); arrived != nullptr && arrived->stepGoal == at)
	{
		return 0xA;
	}
	return 1;
}

// Living::MoveToPos 0x5EC270 (state 1 MOVE_TO_POS): MobileWallHug::MoveTo (0x5EC280) and, only on 0xA, SetTopStateToFinal
// (0x5EC287..0x5EC28E). Returns MoveTo's result (0x5EC293). (aproximado) The +0x24 & 4 test (0x5EC273: 0, no walk) is
// not ported: a villager in the hand is in IN_HAND, not here
uint32_t VillagerMoveToPos(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);

	const auto result = WallHugMoveToResult(entity);
	if (result == 0xA)
	{
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
		// Living::SetTopStateToFinal 0x5ECA80 = Villager::SetTopState(FINAL), vt +0x8E8: the pause roll, the exit of
		// MOVE_TO_POS (ExitMoveToPos 0x5EDDA0, CircleHugInfo::Reset; not ported: taken as 1) and FINAL's exit / entry
		const auto final = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(LivingAction::Index::Final)));
		if (final != VillagerStates::InvalidState)
		{
			ecs::villager::SetTopStateToFinal(entity);
		}
		else
		{
			// a walk set up by openblack's own code with FINAL 0 (the debug tools' "Move To Point"), where the original
			// would SetTopState(0): back to deciding through the compat path
			Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top,
			                                                      VillagerStates::DecideWhatToDo, true);
		}
	}
	return result;
}
// FLYING / IN_HAND: the physics and the hand move the villager; nothing to decide meanwhile
uint32_t VillagerCarried([[maybe_unused]] LivingAction& action)
{
	return 0;
}

// Villager LANDED: PlayAnimThenSetState (0x5ECAC0), the landing clip and then deciding again. openblack has no final
// state for it yet, so DECIDE_WHAT_TO_DO.
uint32_t VillagerLanded(LivingAction& action)
{
	if (ecs::VillagerAnimationDone(Locator::entitiesRegistry::value().ToEntity(action), action.turnsSinceStateChange))
	{
		Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::DecideWhatToDo,
		                                                      true);
	}
	return 0;
}
} // namespace

using ecs::villager::VillagerStateTableEntry;

namespace
{
/// A table row the original has and openblack has not ported: its state function does nothing (0) and warns once per
/// state; the entry, exit and validate slots are empty (as "no function": 1), and LivingActionSystem::VillagerCallEntry
/// / VillagerCallExit / VillagerCallValidate warn once if the original has one there (VillagerOriginalFns.h)
uint32_t TodoState(LivingAction& action)
{
	static std::bitset<256> warned;
	const auto state = static_cast<size_t>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
	if (!warned.test(state))
	{
		warned.set(state);
		SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented state function: {} {} (0x{:X})",
		                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)), state,
		                   k_VillagerStateStrings.at(std::min<size_t>(state, k_VillagerStateStrings.size() - 1)),
		                   ecs::villager::k_OriginalStateFns.at(std::min<size_t>(state, 254)).state);
	}
	return 0;
}

/// Milagros' worship exits keep the old convention (false = it may leave): the row adapts them to the original's
/// (1 = it may leave)
uint32_t OldExit(bool refused)
{
	return refused ? 0u : 1u;
}
} // namespace

static const VillagerStateTableEntry k_TodoEntry = {
    .state = &TodoState,
    .saveState = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented save state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .loadState = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented load state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .field0x50 = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented field0x50 state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .field0x60 = [](LivingAction& action) -> bool {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented field0x60 state function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return false;
    },
    .transitionAnimation = [](LivingAction& action) -> int {
	    SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented transition animation function: {}",
	                       static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                       k_VillagerStateStrings.at(static_cast<size_t>(
	                           Locator::livingActionSystem::value().VillagerGetState(action, LivingAction::Index::Top))));
	    return -1;
    },
};

/// A not-ported row whose exit (+0x20) the original fills with the thunk 0x5B0100 (jmp [vt +0x910]) = Villager::ExitReaction
/// 0x7527A0 (_$E32): the state is a TODO, its exit is the shared one
static VillagerStateTableEntry TodoWithExitReaction()
{
	auto entry = k_TodoEntry;
	entry.exitState = &ecs::villager_reactions::ExitReaction;
	return entry;
}

const static std::array<VillagerStateTableEntry, static_cast<size_t>(VillagerStates::_COUNT)> k_VillagerStateTable = {
    /* INVALID_STATE */ VillagerStateTableEntry {
        .state = &VillagerInvalidState,
    },
    /* MOVE_TO_POS */
    VillagerStateTableEntry {
        .state = &VillagerMoveToPos,
    },
    /* MOVE_TO_OBJECT */ k_TodoEntry,
    /* MOVE_ON_STRUCTURE */ k_TodoEntry,
    // the script states (ECS/Villager/VillagerScript.h): StateInScript 0x5ED9A0, EnterInScript 0x5ED7E0 (vt +0x940),
    // ExitInScript 0x5ED9C0 (vt +0x914); SaveInScript / LoadInScript not ported
    /* IN_SCRIPT */
    {.state = &ecs::villager::StateInScript,
     .entryState = &ecs::villager::EnterInScript,
     .exitState = &ecs::villager::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* IN_DANCE */ k_TodoEntry,
    /* FLEEING_FROM_OBJECT_REACTION */ TodoWithExitReaction(),
    /* LOOKING_AT_OBJECT_REACTION */ TodoWithExitReaction(),
    /* FOLLOWING_OBJECT_REACTION */ TodoWithExitReaction(),
    /* INSPECT_OBJECT_REACTION */ TodoWithExitReaction(),
    /* FLYING */ {.state = &VillagerCarried},
    /* LANDED */ {.state = &VillagerLanded},
    /* LOOK_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* SET_DYING */ k_TodoEntry,
    /* DYING */ k_TodoEntry,
    /* DEAD */ k_TodoEntry,
    // the water's state (Villager::Drowning 0x76A780, ECS/VillagerDrowning). EnterDrowning 0x767410 (`mov eax, 1; ret 8`) and
    // ExitDrowning 0x767420 (`mov eax, 1; ret 4`) only accept
    /* DROWNING */ {.state = &openblack::ecs::VillagerDrowningState,
                    .entryState = [](LivingAction&, VillagerStates, VillagerStates) -> uint32_t { return 1; },
                    .exitState = [](LivingAction&, VillagerStates) -> uint32_t { return 1; }},
    /* DOWNED */ {.state = &VillagerCarried},      // caught by a predator: the animal AI drives it (ECS/AnimalPredators)
    /* BEING_EATEN */ {.state = &VillagerCarried},
    /* GOTO_FOOD_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_FOOD_REACTION */ k_TodoEntry,
    /* GOTO_WOOD_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_WOOD_REACTION */ k_TodoEntry,
    // Living::WaitForAnimation 0x5EC990 (no entry or exit; +0x50 AlwaysReactToTownEmergency; SaveWaitForAnim /
    // LoadWaitForAnim not ported)
    /* WAIT_FOR_ANIMATION */
    {.state = &ecs::villager::WaitForAnimation,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState,
     .field0x50 = k_TodoEntry.field0x50},
    /* IN_HAND */ {.state = &VillagerCarried},
    /* GOTO_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* MOVE_IN_FLOCK */ k_TodoEntry,
    /* MOVE_ALONG_PATH */ k_TodoEntry,
    /* MOVE_ON_PATH */ k_TodoEntry,
    /* FLEEING_AND_LOOKING_AT_OBJECT_REACTION */ k_TodoEntry,
    // V5, carrying (VillagerResources.cpp): no entry, no exit; +0x50 AlwaysReactToTownEmergency (0x5AC990)
    /* GOTO_STORAGE_PIT_FOR_DROP_OFF: Villager::GotoStoragePitForDropOff 0x769620 */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForDropOffState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF: Villager::ArrivesAtStoragePitForDropOff 0x7696D0 (clip 347) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForDropOff,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // V4, the food (VillagerFood.cpp, P-1: the storage pit): no entry; +0x50 AlwaysReactToTownEmergency (0x5AC990)
    /* GOTO_STORAGE_PIT_FOR_FOOD: Villager::GotoStoragePitForFood 0x769830 */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_FOOD: Villager::ArrivesAtStoragePitForFood 0x7698B0 */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_HOME_WITH_FOOD: Villager::ArrivesAtHomeWithFood 0x769B30 (the housewife's, V14); exit ExitAtHome
       0x761B40 */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtHomeWithFood,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // V4, the home (VillagerHome.cpp): the exit of 35..38, 118..121 is ExitAtHome 0x761B40 (LeaveHome unless the next
    // state stays at home, file 0xC0)
    /* GO_HOME: Villager::GoHome 0x760270 = DoGoingHome(37, 238); +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_HOME: Villager::ArrivesHome 0x760930; +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* AT_HOME: Villager::AtHome 0x760B10 = HomeDecideWhatToDo (clip -4: not drawn) */
    VillagerStateTableEntry {
        .state = &ecs::villager::AtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS */ k_TodoEntry,
    /* ARRIVES_AT_BUILDING_SITE */ k_TodoEntry,
    /* BUILDING */ k_TodoEntry,
    /* GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */ k_TodoEntry,
    /* GOTO_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    /* MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    /* ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES */ k_TodoEntry,
    /* FORESTER_MOVE_TO_FOREST */ k_TodoEntry,
    /* FORESTER_GOTO_FOREST */ k_TodoEntry,
    /* FORESTER_ARRIVES_AT_FOREST */ k_TodoEntry,
    /* FORESTER_CHOPS_TREE */ k_TodoEntry,
    /* FORESTER_CHOPS_TREE_FOR_BUILDING */ k_TodoEntry,
    /* FORESTER_FINISHED_FORESTERING */ k_TodoEntry,
    /* ARRIVES_AT_BIG_FOREST */ k_TodoEntry,
    /* ARRIVES_AT_BIG_FOREST_FOR_BUILDING */ k_TodoEntry,
    /* FISHERMAN_ARRIVES_AT_FISHING */ k_TodoEntry,
    /* FISHING */ k_TodoEntry,
    // Living::WaitForCounter 0x5EC310 (no entry or exit): the states that park a villager for a while (the amazed
    // villager of VillagerShield.cpp, SetupWaitForCounter 0x76B060) need it to come back to their final state
    /* WAIT_FOR_COUNTER */ {.state = &ecs::villager::WaitForCounter},
    // the worship states (VillagerWorship.cpp). 58 is the original's footpath walk (SetupMoveToOnFootpath): openblack
    // walks with the WallHug inside 59, so GotoWorshipSiteForWorship sets 59 straight away and 58 is only entered when a
    // reaction's state is popped (PopFromPrevious resumes 59 as 58), where its own state function 0x76BCC0 =
    // GotoWorshipSiteForWorship starts the walk again (aproximado: the footpath walk is not ported).
    // (their exits keep the old convention, false = it may leave: OldExit adapts them)
    /* GOTO_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::GotoWorshipSiteForWorshipState,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::ArrivesAtWorshipSiteForWorship,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* WORSHIPPING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::WorshippingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    /* GOTO_ALTAR_FOR_REST */ k_TodoEntry,
    /* ARRIVES_AT_ALTAR_FOR_REST */ k_TodoEntry,
    /* AT_ALTAR_REST */ k_TodoEntry,
    /* AT_ALTAR_FINISHED_REST */ k_TodoEntry,
    /* RESTART_WORSHIPPING_AT_WORSHIP_SITE */ k_TodoEntry,
    /* RESTART_WORSHIPPING_CREATURE */ k_TodoEntry,
    /* FARMER_ARRIVES_AT_FARM */ k_TodoEntry,
    /* FARMER_PLANTS_CROP */ k_TodoEntry,
    /* FARMER_DIGS_UP_CROP */ k_TodoEntry,
    /* MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION */ k_TodoEntry,
    /* FOOTBALL_WALK_TO_POSITION */ k_TodoEntry,
    /* FOOTBALL_WAIT_FOR_KICK_OFF */ k_TodoEntry,
    /* FOOTBALL_ATTACKER */ k_TodoEntry,
    /* FOOTBALL_GOALIE */ k_TodoEntry,
    /* FOOTBALL_DEFENDER */ k_TodoEntry,
    /* FOOTBALL_WON_GOAL */ k_TodoEntry,
    /* FOOTBALL_LOST_GOAL */ k_TodoEntry,
    /* START_MOVE_TO_PICK_UP_BALL_FOR_DEAD_BALL */ k_TodoEntry,
    /* ARRIVED_AT_PICK_UP_BALL_FOR_DEAD_BALL */ k_TodoEntry,
    /* ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_START */ k_TodoEntry,
    /* ARRIVED_AT_PUT_DOWN_BALL_FOR_DEAD_BALL_END */ k_TodoEntry,
    /* FOOTBALL_MATCH_PAUSED */ k_TodoEntry,
    /* FOOTBALL_WATCH_MATCH */ k_TodoEntry,
    /* FOOTBALL_MEXICAN_WAVE */ k_TodoEntry,
    /* CREATED: Villager::VillagerCreated 0x753DD0 (no entry or exit; +0x50 AlwaysReactToTownEmergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::VillagerCreated,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_IN_ABODE_TO_TRADE */ k_TodoEntry,
    /* ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS */ k_TodoEntry,
    /* MAKE_SCARED_STIFF */ k_TodoEntry,
    /* SCARED_STIFF */ k_TodoEntry,
    /* WORSHIPPING_CREATURE */ k_TodoEntry,
    /* SHEPHERD_LOOK_FOR_FLOCK */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_TO_WATER */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_TO_FOOD */ k_TodoEntry,
    /* SHEPHERD_MOVE_FLOCK_BACK */ k_TodoEntry,
    /* SHEPHERD_DECIDE_WHAT_TO_DO_WITH_FLOCK */ k_TodoEntry,
    /* SHEPHERD_WAIT_FOR_FLOCK */ k_TodoEntry,
    /* SHEPHERD_SLAUGHTER_ANIMAL */ k_TodoEntry,
    /* SHEPHERD_FETCH_STRAY */ k_TodoEntry,
    /* SHEPHERD_GOTO_FLOCK */ k_TodoEntry,
    /* HOUSEWIFE_AT_HOME */ k_TodoEntry,
    /* HOUSEWIFE_GOTO_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_ARRIVES_AT_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_PICKUP_FROM_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_RETURN_HOME_WITH_FOOD */ k_TodoEntry,
    /* HOUSEWIFE_MAKE_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_SERVES_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_CLEARS_AWAY_DINNER */ k_TodoEntry,
    /* HOUSEWIFE_DOES_HOUSEWORK */ k_TodoEntry,
    /* HOUSEWIFE_GOSSIPS_AROUND_STORAGE_PIT */ k_TodoEntry,
    /* HOUSEWIFE_STARTS_GIVING_BIRTH */ k_TodoEntry,
    /* HOUSEWIFE_GIVING_BIRTH */ k_TodoEntry,
    /* HOUSEWIFE_GIVEN_BIRTH */ k_TodoEntry,
    /* CHILD_AT_CRECHE */ k_TodoEntry,
    /* CHILD_FOLLOWS_MOTHER: Villager::ChildFollowsMother 0x7578C0 (VillagerDecide.cpp; no entry or exit; +0x50
       AlwaysReactToTownEmergency: 0xD0D208 = 0x5AC990, _$E32 0x5A4046..0x5A404B) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildFollowsMother,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* CHILD_BECOMES_ADULT: Villager::ChildBecomesAdult 0x757F10 (VillagerAge.cpp; (inferido) only a script sets 115) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildBecomesAdultState,
    },
    /* SITS_DOWN_TO_DINNER */ k_TodoEntry,
    /* EAT_FOOD: Villager::EatFood 0x75C000 (VillagerFood.cpp; clip 254 EatDinner); +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* EAT_FOOD_AT_HOME: Villager::EatFoodAtHome 0x75C090 (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFoodAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* GOTO_BED_AT_HOME: Villager::GotoBedAtHome 0x760B30 (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoBedAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* SLEEPING_AT_HOME: Villager::SleepingAtHome 0x760D70 (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepingAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* WAKE_UP_AT_HOME: Villager::WakeUpAtHome 0x760E50 = jmp GoHome (no code sets 121); +0x50 */
    VillagerStateTableEntry {
        .state = &ecs::villager::WakeUpAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* START_HAVING_SEX */ k_TodoEntry,
    /* HAVING_SEX */ k_TodoEntry,
    /* STOP_HAVING_SEX */ k_TodoEntry,
    /* START_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* STOP_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* WAIT_FOR_DINNER */ k_TodoEntry,
    /* HOMELESS_START: Villager::HomelessStart 0x761320 (VillagerHome.cpp); +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::HomelessStart,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* VAGRANT_START: Villager::VagrantStart 0x76A8D0; +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::VagrantStartState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* MORN_DEATH */ k_TodoEntry,
    /* PERFORM_INSPECTION_REACTION */ k_TodoEntry,
    /* APPROACH_OBJECT_REACTION */ k_TodoEntry,
    /* INITIALISE_TELL_OTHERS_ABOUT_OBJECT */ k_TodoEntry,
    /* TELL_OTHERS_ABOUT_INTERESTING_OBJECT */ k_TodoEntry,
    /* APPROACH_VILLAGER_TO_TALK_TO */ k_TodoEntry,
    /* TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT */ k_TodoEntry,
    /* INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* MOVE_TOWARDS_OBJECT_TO_LOOK_AT */ k_TodoEntry,
    /* INITIALISE_IMPRESSED_REACTION */ k_TodoEntry,
    /* PERFORM_IMPRESSED_REACTION */ k_TodoEntry,
    /* INITIALISE_FIGHT_REACTION */ k_TodoEntry,
    /* PERFORM_FIGHT_REACTION */ k_TodoEntry,
    /* HOMELESS_EAT_DINNER */ k_TodoEntry,
    /* INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_INSPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_CREATURE_REACTION */ k_TodoEntry,
    /* INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_MAGIC_TREE */ k_TodoEntry,
    /* LOOK_AT_MAGIC_TREE */ k_TodoEntry,
    /* DANCE_FOR_EDITING_PURPOSES */ k_TodoEntry,
    /* MOVE_TO_DANCE_POS */ k_TodoEntry,
    /* INITIALISE_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* PERFORM_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* FINISH_RESPECT_CREATURE_REACTION */ k_TodoEntry,
    /* APPROACH_HAND_REACTION */ k_TodoEntry,
    /* FLEEING_FROM_CREATURE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_CREATURE_REACTION */ k_TodoEntry,
    /* WATCH_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* POINT_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    /* DECIDE_WHAT_TO_DO: Villager::DecideWhatToDo 0x7515C0 (VillagerDecide.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::DecideWhatToDo,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO */ k_TodoEntry,
    /* EAT_OUTSIDE */ k_TodoEntry,
    /* RUN_AWAY_FROM_OBJECT_REACTION */ k_TodoEntry,
    /* MOVE_TOWARDS_CREATURE_REACTION */ k_TodoEntry,
    // the shield's state (VillagerShield.cpp); its exit (+0x20) is the thunk 0x5B0100 = Villager::ExitReaction 0x7527A0
    /* AMAZED_BY_MAGIC_SHIELD_REACTION */
    {.state = &ecs::villager_shield::AmazedByMagicShieldReaction, .exitState = &ecs::villager_reactions::ExitReaction},
    /* VILLAGER_GOSSIPS */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ANIMAL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_WORSHIP_SITE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ABODE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FIELD */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FISH_FARM */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_TREE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_BALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_POT */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FOOTBALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_VILLAGER */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_MAGIC_LIVING */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ROCK */ k_TodoEntry,
    /* ARRIVES_AT_ROCK_FOR_WOOD */ k_TodoEntry,
    /* GOT_WOOD_FROM_ROCK */ k_TodoEntry,
    /* REENTER_BUILDING_STATE */ k_TodoEntry,
    /* ARRIVE_AT_PUSH_OBJECT */ k_TodoEntry,
    /* TAKE_WOOD_FROM_TREE */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT */ k_TodoEntry,
    /* TAKE_WOOD_FROM_TREE_FOR_BUILDING */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT_FOR_BUILDING */ k_TodoEntry,
    /* SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* SHEPHERD_TAKES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* SHEPHERD_RELEASES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* DANCE_BUT_NOT_WORSHIP */ k_TodoEntry,
    /* FAINTING_REACTION */ k_TodoEntry,
    /* START_CONFUSED_REACTION */ k_TodoEntry,
    /* CONFUSED_REACTION */ k_TodoEntry,
    /* AFTER_TAP_ON_ABODE */ k_TodoEntry,
    /* WEAK_ON_GROUND */ k_TodoEntry,
    /* SCRIPT_WANDER_AROUND_POSITION */ k_TodoEntry,
    // Villager::ScriptPlayAnim 0x768970, EnterPlayAnim 0x768840 (vt +0x958), ExitPlayAnim 0x7689C0 (vt +0x95C); the clip
    // (ScriptAnimation 0x768A00) is VillagerAnimations' AnimFn::Script; SaveScriptPos / LoadScriptPos not ported
    /* SCRIPT_PLAY_ANIM */
    {.state = &ecs::villager::ScriptPlayAnim,
     .entryState = &ecs::villager::EnterPlayAnim,
     .exitState = &ecs::villager::ExitPlayAnim,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the teleport stones' states (VillagerTeleport.cpp); their exit (+0x20) ExitReactToTeleport 0x766390
    /* GO_TOWARDS_TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::TeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* DANCE_WHILE_REACTING */ TodoWithExitReaction(),
    /* CONTROLLED_BY_CREATURE */ k_TodoEntry,
    /* POINT_AT_DEAD_PERSON */ k_TodoEntry,
    /* GO_TOWARDS_DEAD_PERSON */ k_TodoEntry,
    /* LOOK_AT_DEAD_PERSON */ k_TodoEntry,
    /* MOURN_DEAD_PERSON */ k_TodoEntry,
    /* NOTHING_TO_DO: Villager::NothingToDo 0x760000 (no entry or exit; +0x50 AlwaysReactToTownEmergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::NothingToDo,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_WORKSHOP_FOR_DROP_OFF */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS */ k_TodoEntry,
    /* SHOW_POISONED: Villager::ShowPoisoned 0x75B940 (VillagerFood.cpp; clip 342) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ShowPoisoned,
    },
    /* HIDING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::HidingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    /* CROWD_REACTION */ TodoWithExitReaction(),
    // the fire's states (VillagerFire.cpp). Their entry (+0x10) and exit (+0x20) functions as _$E32 fills them: 216
    // 0x5AA2D9 / 0x5AA2EC, 217 0x5AA421 / 0x5AA434, 218 0x5AA4BE / 0x5AA4CB and 220 0x5AA75D / 0x5AA767 EnterPutOutFire
    // 0x75ADC0 / ExitPutOutFire 0x75AE80; 219 0x5AA611 / 0x5AA642 EnterOnFire 0x75AF30 / ExitOnFire 0x75AF80. 215's
    // exit is the thunk 0x5B0100 = Villager::ExitReaction 0x7527A0
    /* REACT_TO_FIRE */ {.state = &ecs::villager_fire::ReactToFire, .exitState = &ecs::villager_reactions::ExitReaction},
    /* PUT_OUT_FIRE_BY_BEATING */
    {.state = &ecs::villager_fire::PutOutFireByBeating,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* PUT_OUT_FIRE_WITH_WATER */
    {.state = &ecs::villager_fire::PutOutFireWithWater,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* GET_WATER_TO_PUT_OUT_FIRE */
    {.state = &ecs::villager_fire::PutOutFireWithWater,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* ON_FIRE */
    {.state = &ecs::villager_fire::OnFire,
     .entryState = &ecs::villager_fire::EnterOnFire,
     .exitState = &ecs::villager_fire::ExitOnFire},
    /* MOVE_AROUND_FIRE */
    {.state = &ecs::villager_fire::MoveAroundFire,
     .entryState = &ecs::villager_fire::EnterPutOutFire,
     .exitState = &ecs::villager_fire::ExitPutOutFire},
    /* DISCIPLE_NOTHING_TO_DO */ k_TodoEntry,
    /* FOOTBALL_MOVE_TO_BALL */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF */ k_TodoEntry,
    /* BREEDER_DISCIPLE */ k_TodoEntry,
    /* MISSIONARY_DISCIPLE */ k_TodoEntry,
    /* REACT_TO_BREEDER */ k_TodoEntry,
    /* SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER */ k_TodoEntry,
    /* ARTIFACT_DANCE */ k_TodoEntry,
    /* FLEEING_FROM_PREDATOR_REACTION */ k_TodoEntry,
    /* WAIT_FOR_WOOD */ k_TodoEntry,
    /* INSPECT_OBJECT */ k_TodoEntry,
    /* GO_HOME_AND_CHANGE: Villager::GoHomeAndChange 0x761810, exit ExitGoHomeAndChange 0x761980 (VillagerHome.cpp: the
       grown-up child's adult mesh); +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeAndChange,
        .exitState = &ecs::villager::ExitGoHomeAndChange,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* WAIT_FOR_MATE */ k_TodoEntry,
    /* GO_AND_HIDE_IN_NEARBY_BUILDING */ k_TodoEntry,
    /* LOOK_TO_SEE_IF_IT_IS_SAFE */ k_TodoEntry,
    /* SLEEP_IN_TENT: Villager::SleepInTent 0x761AE0 (VillagerHome.cpp; clip 381 and the in / out clip
       SleepInTentIntoOutofAnimation 0x424290 of VillagerAnimationTable.h); +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PAUSE_FOR_A_SECOND: Villager::PauseForASecond 0x76B0B0 (no entry, exit, save or load; +0x50
       AlwaysReactToTownEmergency; its clip function +0x60 PauseForASecondAnimation 0x424080 is in
       VillagerAnimationTable.h) */
    VillagerStateTableEntry {
        .state = &ecs::villager::PauseForASecond,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PANIC_REACTION */ k_TodoEntry,
    /* GET_FOOD_AT_WORSHIP_SITE */ k_TodoEntry,
    /* GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY */ k_TodoEntry,
    /* CONGREGATE_IN_TOWN_AFTER_EMERGENCY */ k_TodoEntry,
    /* SCRIPT_IN_CROWD */ k_TodoEntry,
    /* GO_AND_CHILLOUT_OUTSIDE_HOME: Villager::GoAndChilloutOutsideHome 0x76B3F0 (VillagerDecide.cpp) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutOutsideHome,
    },
    /* SIT_AND_CHILLOUT: Villager::SitAndChillout 0x76B4E0, entry EnterSitAndChillOut 0x76B570 (its clip functions +0x60
       SitDownAnimation 0x424210 / +0x70 SitDownIntoOutOfAnimation 0x4243A0 are in VillagerAnimationTable.h) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SitAndChillout,
        .entryState = &ecs::villager::EnterSitAndChillOut,
    },
    /* SCRIPT_GO_AND_MOVE_ALONG_PATH */ k_TodoEntry,
    // (k_VillagerStateStrings mislabels 248..254; the names here are the enum's / info.dat's)
    /* 248 GO_HOME_FROM_WORSHIP: Villager::GoHomeFromWorship 0x761B70 = DoGoingHome(249, 250) (VillagerHome.cpp); exit
       ExitAtHome 0x761B40; +0x50 AlwaysReactToTownEmergency */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t {
            return ecs::villager::DoGoingHome(Locator::entitiesRegistry::value().ToEntity(action),
                                              VillagerStates::ArrivesHomeFromWorship, VillagerStates::SleepInTentFromWorship);
        },
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 249 ARRIVES_HOME_FROM_WORSHIP: ArrivesHomeFromWorship 0x76B7E0 = jmp ArrivesHome 0x760930; exit ExitAtHome */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 250 SLEEP_IN_TENT_FROM_WORSHIP: SleepInTentFromWorship 0x76B7F0 = jmp SleepInTent 0x761AE0 */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY (0x766380 = a jmp to 201's; exit ExitReactToTeleport 0x766390) */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* 252 GO_AND_CHILLOUT_IN_TOWN: Villager::GoAndChilloutInTown 0x76B590 (VillagerDecide.cpp; only scripts set it) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutInTown,
    },
    /* 253 WAIT_FOR_ARTIFACT_DANCE */ k_TodoEntry,
    /* 254 BREEDER_JUST_LANDED */ k_TodoEntry,
};

LivingActionSystem::LivingActionSystem()
{
	ecs::living::RegisterPhysicsHandlers();
}

void LivingActionSystem::Update()
{
	auto& registry = Locator::entitiesRegistry::value();
	const uint32_t turn = ecs::villager::CurrentTurn(); // g_game +0x205A40
	ecs::villager::RunDebugHooks(turn);

	// Living::ProcessLiving 0x5EC810 for the villagers: ProcessReaction 0x5F1270 and ProcessState (vt +0x620) one by one.
	// (aproximado) the original runs one list of villagers and animals (g_game +0x205BBC); openblack runs the villagers
	// here in the registry's order and the animals after them (ecs::animal_ai), and the walk of all of them before
	// (PathfindingSystem, the original's MoveToPos state function)
	std::vector<entt::entity> villagers;
	registry.Each<const Villager, const LivingAction>(
	    [&villagers](entt::entity entity, const Villager&, const LivingAction&) { villagers.push_back(entity); });
	for (const auto entity : villagers)
	{
		if (!registry.Valid(entity) || !registry.AllOf<Villager, LivingAction>(entity))
		{
			continue;
		}
		ecs::villager::ProcessReaction(entity);
		ecs::villager::ProcessState(entity, turn);
	}
	// the deaths of the turn (TODO(V12): VillagerDead keeps them alive in the dying states)
	ecs::villager::FlushDeaths();
}

VillagerStates LivingActionSystem::VillagerGetState(const LivingAction& action, LivingAction::Index index) const
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

void LivingActionSystem::VillagerSetState(LivingAction& action, LivingAction::Index index, VillagerStates state,
                                          bool skipTransition) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);
	const auto previousState = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	SPDLOG_LOGGER_TRACE(spdlog::get("ai"), "Villager #{}: Setting state {} -> {}", static_cast<int>(entity),
	                    k_VillagerStateStrings.at(static_cast<size_t>(previousState)),
	                    k_VillagerStateStrings.at(static_cast<size_t>(state)));

	if (index != LivingAction::Index::Top)
	{
		// Villager::SetState 0x753690 (the PREVIOUS rule and AdjustTownModifier)
		ecs::villager::SetState(entity, index, state);
		return;
	}
	if (!skipTransition)
	{
		// Villager::SetTopState 0x752010 (the pause, the exits, the entry, 0x2E / 0x2F, the speed and the clips)
		ecs::villager::SetTopState(entity, state);
		return;
	}
	// The changes that bypass the exit and entry functions (the hand, the physics, the animals, the water, MOVE_TO_POS
	// and LANDED): Villager::SetState(0, s) (FINAL cleared with its town modifier, +0x90 = 0) and the clips and speed
	// as before. (aproximado) in the original those go through SetTopState with their own entry / exit functions
	// (EnterInHand 0x76AFE0, ExitInHand 0x76B000, ...), not ported yet. Repeating the same order changes nothing, as
	// openblack had it
	if (previousState == state)
	{
		return;
	}
	ecs::villager::SetState(entity, LivingAction::Index::Top, state);
	ecs::OnVillagerStateChanged(entity, previousState, state);
}

uint32_t LivingActionSystem::VillagerCallState(LivingAction& action, LivingAction::Index index) const
{
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.state;
	if (!callback)
	{
		return 0;
	}
	return callback(action);
}

namespace
{
enum class Slot : uint8_t
{
	Entry,
	Exit,
	Validate,
};

/// One warning per state and slot when the original has a function there and the row has none
void WarnMissing(LivingAction& action, VillagerStates row, Slot slot)
{
	static std::array<std::bitset<256>, 3> warned;
	const auto index = std::min<size_t>(static_cast<size_t>(row), 254);
	const auto& original = ecs::villager::k_OriginalStateFns.at(index);
	const uint32_t address = slot == Slot::Entry ? original.entry : slot == Slot::Exit ? original.exit : original.validate;
	auto& bits = warned.at(static_cast<size_t>(slot));
	if (address == 0 || bits.test(index))
	{
		return;
	}
	bits.set(index);
	SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented {} function of {} {} (0x{:X}): taken as 1",
	                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                   slot == Slot::Entry ? "entry" : slot == Slot::Exit ? "exit" : "validate", index,
	                   k_VillagerStateStrings.at(index), address);
}
} // namespace

uint32_t LivingActionSystem::VillagerCallEntry(LivingAction& action, VillagerStates row, VillagerStates final,
                                               VillagerStates next) const
{
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(row)).entryState;
	if (!callback)
	{
		WarnMissing(action, row, Slot::Entry);
		return 1;
	}
	return callback(action, final, next);
}

uint32_t LivingActionSystem::VillagerCallExit(LivingAction& action, VillagerStates row, VillagerStates next) const
{
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(row)).exitState;
	if (!callback)
	{
		WarnMissing(action, row, Slot::Exit);
		return 1;
	}
	return callback(action, next);
}

int LivingActionSystem::VillagerCallOutOfAnimation(LivingAction& action, LivingAction::Index index) const
{
	const auto state = action.states.at(static_cast<size_t>(index));
	const auto& entry = k_VillagerStateTable.at(static_cast<size_t>(state));
	const auto& callback = entry.transitionAnimation;
	if (!callback)
	{
		return -1;
	}
	return callback(action);
}

bool LivingActionSystem::VillagerCallValidate(LivingAction& action, LivingAction::Index index) const
{
	const auto state = static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	const auto& callback = k_VillagerStateTable.at(static_cast<size_t>(state)).validate;
	// the +0x80 column of 0xD09198: Villager::ReactionValidate 0x756A00 in the reaction rows (201, 202, 251, 215-218,
	// 220, 6-30, 140-196...: every row whose original validate is 0x756A00, VillagerOriginalFns.h). Villager::ProcessState
	// calls this slot for +0x8C (0x74FF91) and +0x8D (0x74FFD9) before CallState; the result is unused there
	if (!callback && ecs::villager::k_OriginalStateFns.at(std::min<size_t>(static_cast<size_t>(state), 254)).validate ==
	                     0x756A00)
	{
		return ecs::villager_reactions::ReactionValidate(action);
	}
	if (!callback)
	{
		WarnMissing(action, state, Slot::Validate);
		return false;
	}
	return callback(action);
}
