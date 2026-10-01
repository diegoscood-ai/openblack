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

#include "Common/RandomNumberManager.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerOriginalFns.h"
#include "ECS/Villager/VillagerStateTable.h"
#include "VillagerFire.h"
#include "VillagerReactions.h"
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

// Wander behaviour: how far (in world units) an idle villager may roam from
// its village centre when picking a destination.
static constexpr float k_WanderRadius = 40.0f;
// Idle pause (in turns) before an arrived/abandoned villager picks a new destination.
static constexpr uint16_t k_WanderCooldownTurns = 20;
// Keep goals clear of the map edges so the pathfinder's neighbouring-cell lookups stay in
// bounds (the movement grid is MapInterface::k_GridSize cells, each 10 world units wide).
static constexpr float k_WanderWorldBoundMin = 30.0f;
static constexpr float k_WanderWorldBoundMax = 5090.0f;

namespace
{
// Pick a random nearby point and hand it to the PathfindingSystem the same way the
// debug "Move To Point" tool does (see Debug/PathFinding.cpp), then wait in MoveToPos.
uint32_t VillagerDecideWhatToDo(LivingAction& action)
{
	// Villager::DecideWhatToDo's worship check (0x76BA60, VillagerWorship.cpp): it runs before the idle wander, as the
	// original runs it before the "nothing to do" branch
	if (ecs::villager_worship::CheckNeededForWorship(Locator::entitiesRegistry::value().ToEntity(action)))
	{
		return 0;
	}

	if (action.turnsSinceStateChange < k_WanderCooldownTurns)
	{
		// TODO(#863): play a "catch breath" idle animation here (lean forward, breathe) during the cooldown
		// once villager animation playback exists. The transitionAnimation hook in k_VillagerStateTable
		// is the intended home for it; the Mesh component has no animation state today.
		return 0;
	}

	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);

	const auto& transform = registry.Get<Transform>(entity);
	auto& wallHug = registry.Get<WallHug>(entity);
	auto& rng = Locator::rng::value();

	const float angle = rng.NextValue(0.0f, glm::two_pi<float>());
	const float distance = rng.NextValue(0.0f, k_WanderRadius);
	const auto& villager = registry.Get<Villager>(entity);
	auto origin = glm::xz(transform.position); // fallback if the villager has no valid town
	if (villager.town != entt::null && registry.Valid(villager.town) && registry.AllOf<Transform>(villager.town))
	{
		origin = glm::xz(registry.Get<Transform>(villager.town).position);
	}
	auto goal = origin + glm::vec2(glm::cos(angle), glm::sin(angle)) * distance;
	goal = glm::clamp(goal, glm::vec2(k_WanderWorldBoundMin), glm::vec2(k_WanderWorldBoundMax));

	wallHug.goal = goal;
	wallHug.step = glm::vec2(0.0f); // force a fresh step to be computed on the next pathfinding turn
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	registry.Remove<WallHugObjectReference>(entity);
	registry.Assign<MoveStateLinearTag>(entity);

	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, VillagerStates::MoveToPos, true);
	// (inventado, puente hasta V2) the original's walks have a final state (Living::SetupMoveToWithHug 0x5F2890 sets
	// FINAL); the idle wander takes 209 NOTHING_TO_DO, the original's idle state (DecideWhatToDo -> SetupNothingToDo
	// 0x753B50): its row takes reactions (file 0xEC 1, like row 0's; 163's is 0) and resumes 163 (file 0x20), so a
	// reaction's StorePreviousState 0x763470 / PopFromPrevious 0x751E50 come back to deciding, not to row 0's resume 0
	// (INVALID_STATE). 209's state function is not ported: the arrival goes back to 163 (VillagerMoveToPos)
	ecs::villager::SetState(entity, LivingAction::Index::Final, VillagerStates::NothingToDo);
	return 0;
}

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
		if (final != VillagerStates::InvalidState && final != VillagerStates::NothingToDo)
		{
			ecs::villager::SetTopStateToFinal(entity);
		}
		else
		{
			// (inventado, puente hasta V2) the idle wander's FINAL 209 (its state function is not ported) or a walk set
			// up by openblack's own code with FINAL 0 (the debug tools), where the original would SetTopState(FINAL): back
			// to deciding through the compat path
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
    /* IN_SCRIPT */ k_TodoEntry,
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
    /* WAIT_FOR_ANIMATION */ k_TodoEntry,
    /* IN_HAND */ {.state = &VillagerCarried},
    /* GOTO_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* MOVE_IN_FLOCK */ k_TodoEntry,
    /* MOVE_ALONG_PATH */ k_TodoEntry,
    /* MOVE_ON_PATH */ k_TodoEntry,
    /* FLEEING_AND_LOOKING_AT_OBJECT_REACTION */ k_TodoEntry,
    /* GOTO_STORAGE_PIT_FOR_DROP_OFF */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF */ k_TodoEntry,
    /* GOTO_STORAGE_PIT_FOR_FOOD */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_FOOD */ k_TodoEntry,
    /* ARRIVES_AT_HOME_WITH_FOOD */ k_TodoEntry,
    /* GO_HOME */ k_TodoEntry,
    /* ARRIVES_HOME */ k_TodoEntry,
    /* AT_HOME */ k_TodoEntry,
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
    /* WAIT_FOR_COUNTER */ k_TodoEntry,
    // the worship states (VillagerWorship.cpp). 58 is the original's footpath walk (SetupMoveToOnFootpath): openblack
    // walks with the WallHug inside 59, so GotoWorshipSiteForWorship sets 59 straight away and 58 is never entered
    // (aproximado: the footpath walk is not ported).
    // (their exits keep the old convention, false = it may leave: OldExit adapts them)
    /* GOTO_WORSHIP_SITE_FOR_WORSHIP */
    {.exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
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
    /* CHILD_FOLLOWS_MOTHER */ k_TodoEntry,
    /* CHILD_BECOMES_ADULT */ k_TodoEntry,
    /* SITS_DOWN_TO_DINNER */ k_TodoEntry,
    /* EAT_FOOD */ k_TodoEntry,
    /* EAT_FOOD_AT_HOME */ k_TodoEntry,
    /* GOTO_BED_AT_HOME */ k_TodoEntry,
    /* SLEEPING_AT_HOME */ k_TodoEntry,
    /* WAKE_UP_AT_HOME */ k_TodoEntry,
    /* START_HAVING_SEX */ k_TodoEntry,
    /* HAVING_SEX */ k_TodoEntry,
    /* STOP_HAVING_SEX */ k_TodoEntry,
    /* START_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* STOP_HAVING_SEX_AT_HOME */ k_TodoEntry,
    /* WAIT_FOR_DINNER */ k_TodoEntry,
    /* HOMELESS_START */ k_TodoEntry,
    /* VAGRANT_START */ k_TodoEntry,
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
    /* DECIDE_WHAT_TO_DO */
    VillagerStateTableEntry {
        .state = &VillagerDecideWhatToDo,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO */ k_TodoEntry,
    /* EAT_OUTSIDE */ k_TodoEntry,
    /* RUN_AWAY_FROM_OBJECT_REACTION */ k_TodoEntry,
    /* MOVE_TOWARDS_CREATURE_REACTION */ k_TodoEntry,
    /* AMAZED_BY_MAGIC_SHIELD_REACTION */ k_TodoEntry,
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
    /* SCRIPT_PLAY_ANIM */ k_TodoEntry,
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
    /* NOTHING_TO_DO */ k_TodoEntry,
    /* ARRIVES_AT_WORKSHOP_FOR_DROP_OFF */ k_TodoEntry,
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS */ k_TodoEntry,
    /* SHOW_POISONED */ k_TodoEntry,
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
    /* GO_HOME_AND_CHANGE */ k_TodoEntry,
    /* WAIT_FOR_MATE */ k_TodoEntry,
    /* GO_AND_HIDE_IN_NEARBY_BUILDING */ k_TodoEntry,
    /* LOOK_TO_SEE_IF_IT_IS_SAFE */ k_TodoEntry,
    /* SLEEP_IN_TENT */ k_TodoEntry,
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
    /* GO_AND_CHILLOUT_OUTSIDE_HOME */ k_TodoEntry,
    /* SIT_AND_CHILLOUT */ k_TodoEntry,
    /* SCRIPT_GO_AND_MOVE_ALONG_PATH */ k_TodoEntry,
    // (k_VillagerStateStrings mislabels 248..254; the names here are the enum's / info.dat's)
    /* 248 GO_HOME_FROM_WORSHIP */ {.state = &ecs::villager_worship::GoHomeFromWorship},
    /* 249 ARRIVES_HOME_FROM_WORSHIP */ k_TodoEntry,
    /* 250 SLEEP_IN_TENT_FROM_WORSHIP */ k_TodoEntry,
    /* 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY (0x766380 = a jmp to 201's; exit ExitReactToTeleport 0x766390) */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* 252 GO_AND_CHILLOUT_IN_TOWN */ k_TodoEntry,
    /* 253 WAIT_FOR_ARTIFACT_DANCE */ k_TodoEntry,
    /* 254 BREEDER_JUST_LANDED */ k_TodoEntry,
};

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
	if (!callback)
	{
		WarnMissing(action, state, Slot::Validate);
		return false;
	}
	return callback(action);
}
