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

#include <string>

#include <fmt/format.h>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/vec_swizzle.hpp>
#include <glm/trigonometric.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "Common/ProcessLogOnce.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/LivingAngle.h"
#include "ECS/LivingFootpath.h"
#include "ECS/LivingTurn.h"
#include "ECS/LivingWalkPath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerAge.h"
#include "ECS/Villager/VillagerBirth.h"
#include "ECS/Villager/VillagerBreeding.h"
#include "ECS/Villager/VillagerBuild.h"
#include "ECS/Villager/VillagerChild.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerDisciple.h"
#include "ECS/Villager/VillagerEmergency.h"
#include "ECS/Villager/VillagerFarmer.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "ECS/Villager/VillagerFlock.h"
#include "ECS/Villager/VillagerFood.h"
#include "ECS/Villager/VillagerForester.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerInteract.h"
#include "ECS/Villager/VillagerMissionary.h"
#include "ECS/Villager/VillagerMourning.h"
#include "ECS/Villager/VillagerResources.h"
#include "ECS/Villager/VillagerSatisfy.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerSoul.h"
#include "ECS/Villager/VillagerStateFunctions.h"
#include "ECS/Villager/VillagerStateTable.h"
#include "ECS/Villager/VillagerTrader.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerAnimations.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/WalkArrival.h"
#include "Enums.h"
#include "GameClock.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerArtefactDance.h"
#include "VillagerCover.h"
#include "VillagerCreature.h"
#include "VillagerCreatureDance.h"
#include "VillagerDance.h"
#include "VillagerFire.h"
#include "VillagerReactions.h"
#include "VillagerResourceReactions.h"
#include "VillagerScript.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"
#include "VillagerWorship.h"

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
// state 1 MOVE_TO_POS: the walk and, only on 0xA, the final state. Returns the walk's result
uint32_t VillagerMoveToPos(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);

	// a villager in a hand does not walk (0). (approximate) only the local hand is asked (HandSystem::GetHeldObject)
	if (Locator::handSystem::has_value())
	{
		if (const auto held = Locator::handSystem::value().GetHeldObject(); held.has_value() && *held == entity)
		{
			return 0;
		}
	}
	ecs::living_turn::MoveToStep(entity);
	const auto result = ecs::living_turn::WallHugMoveToResult(entity);
	if (result == 0xA)
	{
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
		// SetTopStateToFinal: the pause roll, the exit of MOVE_TO_POS (VillagerExitMoveToPos) and FINAL's exit / entry
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
// the exit of MOVE_TO_POS, also MOVE_TO_OBJECT's and the football walk to the ball's: the walker lets go of the circle
// it went round or was heading for, whatever comes next. Always 1. The walk's target object, which the original clears
// here too, openblack's villagers do not keep (their walk to an object is not ported)
uint32_t VillagerExitMoveToPos(LivingAction& action, VillagerStates /*next*/)
{
	auto& registry = Locator::entitiesRegistry::value();
	registry.Remove<WallHugObjectReference>(registry.ToEntity(action));
	return 1;
}

// 29 MOVE_ON_PATH: the walk's step, then the footpath's turn on what it gave (living_footpath::MoveOnFootpath).
// Returns its result
uint32_t VillagerMoveOnFootpath(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);
	ecs::living_turn::MoveToStep(entity);
	const auto result = ecs::living_turn::WallHugMoveToResult(entity);
	if (result == 0xA)
	{
		// the arrival is taken: the next walk or the final state starts afresh, as MOVE_TO_POS's
		registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	}
	return ecs::living_footpath::MoveOnFootpath(entity, result);
}

// FLYING / IN_HAND: the physics and the hand move the villager; nothing to decide meanwhile
uint32_t VillagerCarried([[maybe_unused]] LivingAction& action)
{
	return 0;
}

// 24 IN_HAND: every turn in a hand, a creature's too, its home shows it is held; in the player's hand it takes up the
// work of what it is held near (ecs::villager::DiscipleInHand); otherwise as VillagerCarried
uint32_t VillagerInHand(LivingAction& action)
{
	const auto entity = Locator::entitiesRegistry::value().ToEntity(action);
	ecs::villager::MarkHomeOfHeld(entity);
	ecs::villager::DiscipleInHand(entity);
	return 0;
}

// IN_HAND's exit: the hand lets the villager go only into a flight, a landing or a state of dying, and then its
// home is not marked any more
uint32_t VillagerExitInHand(LivingAction& action, VillagerStates next)
{
	if (!ecs::villager_animation::HandLetsGoInto(next))
	{
		return 0;
	}
	ecs::villager::UnmarkHomeOfHeld(Locator::entitiesRegistry::value().ToEntity(action));
	return 1;
}

// 11 LANDED. Its clip is VillagerLandedClip. Always 1
uint32_t VillagerLanded(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.ToEntity(action);
	auto* v = registry.TryGet<Villager>(entity);
	if (v == nullptr)
	{
		return 1;
	}
	// still "in the hand" (put down gently, the physics kept the flag) and a disciple
	if ((v->flags & Villager::k_FlagInHand) != 0 && (v->flags & Villager::k_FlagDisciple) != 0)
	{
		const uint8_t disciple = v->discipleType;
		// on the first turn the hand's player's creature may copy the disciple made, and a disciple made in the hand
		// moves the player's alignment
		if (action.turnsSinceStateChange == 0)
		{
			ecs::villager::ShowDiscipleToCreature(entity);
			ecs::villager::DiscipleTouchDown(entity);
		}
		// wait for the landing clip (return 1)
		if (!ecs::VillagerAnimationDone(entity, action.turnsSinceStateChange))
		{
			return 1;
		}
		// the in-hand reactions it started go; the in-hand flag off
		ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(entity, openblack::Reaction::ReactToVillagerInHand);
		v->flags = static_cast<uint16_t>(v->flags & ~Villager::k_FlagInHand);
		// something round it to work at: it sets about its work (its start state, or INSPECT_OBJECT); else the plain
		// landing below
		if (disciple != 0 && ecs::villager::DiscipleLanded(entity) == 1)
		{
			return 1;
		}
	}
	// the in-hand reactions it started go; the clip, then DECIDE_WHAT_TO_DO
	ecs::effects::reactions::RemoveAllReactionsOfTypeInitiatedBy(entity, openblack::Reaction::ReactToVillagerInHand);
	ecs::villager::PlayAnimThenSetState(entity, VillagerStates::DecideWhatToDo);
	return 1;
}
} // namespace

using ecs::villager::VillagerStateTableEntry;

namespace
{
/// A table row the original has and openblack has not ported: its state function does nothing (0) and warns once per
/// state; the entry, exit and validate slots are empty (as "no function": 1), and LivingActionSystem::VillagerCallEntryState
/// / VillagerCallExitState / VillagerCallValidate warn once if the original has one there (VillagerStateFunctions.h)
uint32_t TodoState(LivingAction& action)
{
	const auto state = static_cast<size_t>(action.states.at(static_cast<size_t>(LivingAction::Index::Top)));
	if (log_once::First("ai/unported-state/" + std::to_string(state)))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented state function: {} {}",
		                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)), state,
		                   k_VillagerStateStrings.at(std::min<size_t>(state, k_VillagerStateStrings.size() - 1)));
	}
	return 0;
}

/// the worship exits keep the old convention (false = it may leave): the row adapts them to the original's
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
    // the town emergency's answer has no default: villager::ReactsToTownEmergency reads the original's column
    // for every row, ported or not (k_TownEmergencyReaction, VillagerStateFunctions.h)
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

/// A not-ported row whose exit is the shared ExitReaction: the state is a TODO, its exit is the shared one
static VillagerStateTableEntry TodoWithExitReaction()
{
	auto entry = k_TodoEntry;
	entry.exitState = &ecs::villager_reactions::ExitReaction;
	return entry;
}

/// A not-ported row whose state function walks (living_turn::k_WalkStates: it reaches the walk directly or through
/// MOVE_TO_POS, the move to an object or the dance): the walk's step
/// (living_turn::MoveToStep), then the TODO. (approximate) The rest of the function and the use of MoveTo's result are
/// not ported
static VillagerStateTableEntry TodoWalk(VillagerStateTableEntry entry = k_TodoEntry)
{
	entry.state = [](LivingAction& action) -> uint32_t {
		ecs::living_turn::MoveToStep(Locator::entitiesRegistry::value().ToEntity(action));
		return TodoState(action);
	};
	return entry;
}

/// A not-ported row whose state function walks and whose exit is MOVE_TO_POS's (VillagerExitMoveToPos)
static VillagerStateTableEntry TodoWalkWithExitMoveToPos()
{
	auto entry = TodoWalk();
	entry.exitState = &VillagerExitMoveToPos;
	return entry;
}

/// 29 MOVE_ON_PATH's row, the walk along a footpath (ECS/LivingFootpath.h): the entry always lets it in, the exit
/// leaves the node; save / load not ported (k_TodoEntry's)
static VillagerStateTableEntry MoveOnPathRow()
{
	auto entry = k_TodoEntry;
	entry.state = &VillagerMoveOnFootpath;
	entry.entryState = [](LivingAction& /*action*/, VillagerStates /*final*/, VillagerStates /*next*/) -> uint32_t {
		return 1;
	};
	entry.exitState = [](LivingAction& action, VillagerStates /*next*/) -> uint32_t {
		return ecs::living_footpath::ExitMoveOnPath(Locator::entitiesRegistry::value().ToEntity(action));
	};
	return entry;
}

const static std::array<VillagerStateTableEntry, static_cast<size_t>(VillagerStates::_COUNT)> k_VillagerStateTable = {
    /* INVALID_STATE */ VillagerStateTableEntry {
        .state = &VillagerInvalidState,
    },
    /* MOVE_TO_POS: the plain walks' validate (VillagerCallValidate) */
    VillagerStateTableEntry {
        .state = &VillagerMoveToPos,
        .exitState = &VillagerExitMoveToPos,
    },
    /* MOVE_TO_OBJECT */ TodoWalkWithExitMoveToPos(),
    /* MOVE_ON_STRUCTURE */ TodoWalk(),
    // the script states (villager_script, VillagerScript.h); save / load not ported
    /* IN_SCRIPT */
    {.state = &ecs::villager_script::InScript,
     .entryState = &ecs::villager_script::EnterInScript,
     .exitState = &ecs::villager_script::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the scripts' dance (VillagerDance.cpp), its walk to its place through MOVE_TO_POS's step; save / load not ported
    /* IN_DANCE */
    {.state = [](LivingAction& action) -> uint32_t { return ecs::villager_dance::InDance(action, &VillagerMoveToPos); },
     .entryState = &ecs::villager_dance::EnterInDance,
     .exitState = &ecs::villager_dance::ExitInDance,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // running from and watching a miracle (VillagerReactions.cpp): the shared ExitReaction; validate ReactionValidate
    // (VillagerCallValidate)
    /* FLEEING_FROM_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_reactions::Fleeing,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* LOOKING_AT_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_reactions::Watching,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FOLLOWING_OBJECT_REACTION */ TodoWithExitReaction(),
    /* INSPECT_OBJECT_REACTION */ TodoWithExitReaction(),
    /* FLYING */ {.state = &VillagerCarried},
    /* LANDED */ {.state = &VillagerLanded},
    /* LOOK_AT_FLYING_OBJECT_REACTION */ k_TodoEntry,
    // the death (ECS/Villager/VillagerDeath.cpp): no entry; DEAD's exit can never leave
    /* SET_DYING */
    VillagerStateTableEntry {
        .state = &ecs::villager::SetDyingState,
    },
    /* DYING */
    VillagerStateTableEntry {
        .state = &ecs::villager::Dying,
    },
    /* DEAD */
    VillagerStateTableEntry {
        .state = &ecs::villager::Dead,
        .exitState = &ecs::villager::CannotExitState,
    },
    // the water's state (ECS/VillagerDrowning). Its entry and exit only accept
    /* DROWNING */
    {.state = &openblack::ecs::VillagerDrowningState,
     .entryState = [](LivingAction&, VillagerStates, VillagerStates) -> uint32_t { return 1; },
     .exitState = [](LivingAction&, VillagerStates) -> uint32_t { return 1; }},
    /* DOWNED */ {.state = &VillagerCarried}, // caught by a predator: the animal AI drives it (ECS/AnimalPredators)
    /* BEING_EATEN */ {.state = &VillagerCarried},
    // reactions 7 / 12 (VillagerResourceReactions.cpp): the shared ExitReaction; validate ReactionValidate
    // (VillagerCallValidate); always reacts to the town emergency
    /* GOTO_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::GotoFoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_FOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::ArrivesAtFoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* GOTO_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::GotoWoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_WOOD_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_resource_reactions::ArrivesAtWoodReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // no entry or exit; always reacts to the town emergency; save / load not ported
    /* WAIT_FOR_ANIMATION */
    {.state = &ecs::villager::WaitForAnimation,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState,
     .field0x50 = k_TodoEntry.field0x50},
    /* IN_HAND, entry EnterInHand (VillagerDisciple.cpp: the disciple it had as it was picked up) */
    {.state = &VillagerInHand, .entryState = &ecs::villager::EnterInHand, .exitState = &VillagerExitInHand},
    /* GOTO_PICKUP_BALL_REACTION */ k_TodoEntry,
    /* ARRIVES_AT_PICKUP_BALL_REACTION */ k_TodoEntry,
    // no entry, exit or validate (ECS/Villager/VillagerFlock.h); save / load not ported (k_TodoEntry's)
    /* MOVE_IN_FLOCK */
    {.state = &ecs::villager::MoveInFlock, .saveState = k_TodoEntry.saveState, .loadState = k_TodoEntry.loadState},
    // entry EnterInScript, exit ExitInScript (villager_script; the state ECS/LivingWalkPath.h); save / load not ported
    // (k_TodoEntry's)
    /* MOVE_ALONG_PATH */
    {.state = &ecs::living::MoveAlongPath,
     .entryState = &ecs::villager_script::EnterInScript,
     .exitState = &ecs::villager_script::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* MOVE_ON_PATH */ MoveOnPathRow(),
    // watching a miracle after running from it: the same state as LOOKING_AT_OBJECT_REACTION
    /* FLEEING_AND_LOOKING_AT_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_reactions::Watching,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // carrying (VillagerResources.cpp): no entry, no exit; always reacts to the town emergency
    /* GOTO_STORAGE_PIT_FOR_DROP_OFF */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForDropOffState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_DROP_OFF (clip 347) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForDropOff,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the food (VillagerFood.cpp, the storage pit): no entry; always reacts to the town emergency
    /* GOTO_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_STORAGE_PIT_FOR_FOOD */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_HOME_WITH_FOOD (the housewife's); exit ExitAtHome */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtHomeWithFood,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the home (VillagerHome.cpp): the exit of 35..38, 118..121 is ExitAtHome (it leaves home unless the next
    // state stays at home)
    /* GO_HOME = DoGoingHome(37, 238); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_HOME: always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* AT_HOME: the home decision (clip -4: not drawn) */
    VillagerStateTableEntry {
        .state = &ecs::villager::AtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    // the builders (VillagerBuild.cpp): entry EnterBuilding (0 = refused -> 163), exit ExitBuilding; always reacts to
    // the town emergency; save / load not ported
    /* ARRIVES_AT_STORAGE_PIT_FOR_BUILDING_MATERIALS (clip 340) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtStoragePitForBuildingMaterials,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_BUILDING_SITE (clip 348) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtBuildingSite,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* BUILDING (the building clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::BuildingState,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the worship site's supply errands (VillagerWorship.cpp), all with the exit ExitMoveToWorshipSiteWithSupplies
    // (the old convention, adapted by OldExit)
    /* GOTO_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */
    {.state = &ecs::villager_worship::GotoStoragePitForWorshipSuppliesState,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSiteWithSupplies(a, n)); }},
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORSHIP_SUPPLIES */
    {.state = &ecs::villager_worship::ArrivesAtStoragePitForWorshipSupplies,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSiteWithSupplies(a, n)); }},
    /* GOTO_WORSHIP_SITE_WITH_SUPPLIES */
    {.state = &ecs::villager_worship::GotoWorshipSiteWithSuppliesState,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSiteWithSupplies(a, n)); }},
    /* MOVE_TO_WORSHIP_SITE_WITH_SUPPLIES */
    {.state = &ecs::villager_worship::MoveToWorshipSiteWithSupplies,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSiteWithSupplies(a, n)); }},
    /* ARRIVES_AT_WORSHIP_SITE_WITH_SUPPLIES */
    {.state = &ecs::villager_worship::ArrivesAtWorshipSiteWithSupplies,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSiteWithSupplies(a, n)); }},
    // the foresters (VillagerForester.cpp): exit ExitForesting for 47-50 and 52; always reacts to the town
    // emergency
    /* FORESTER_MOVE_TO_FOREST (MOVE_TO_POS first; the plain walks' validate, VillagerCallValidate) */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t {
	        return ecs::villager::ForesterMoveToForest(action, VillagerMoveToPos(action));
        },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_GOTO_FOREST: the wood desire check */
    VillagerStateTableEntry {
        .state = &ecs::villager::ForesterGotoForest,
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_ARRIVES_AT_FOREST (clip 217) */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t { return ecs::villager::ForesterArrivesAtForest(action); },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FORESTER_CHOPS_TREE (clip 217) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ForesterChopsTree,
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game: no code changes to this state
    /* FORESTER_CHOPS_TREE_FOR_BUILDING */ k_TodoEntry,
    /* FORESTER_FINISHED_FORESTERING */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t { return ecs::villager::ForesterFinishedForestering(action); },
        .exitState = &ecs::villager::ExitForesting,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_AT_BIG_FOREST (no entry, no exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesAtBigForest,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game
    /* ARRIVES_AT_BIG_FOREST_FOR_BUILDING */ k_TodoEntry,
    // the fishermen (VillagerFisherman.cpp): entry EnterFishing, exit ExitFishing; always reacts to the town
    // emergency
    /* FISHERMAN_ARRIVES_AT_FISHING */
    VillagerStateTableEntry {
        .state = &ecs::villager::FishermanArrivesAtFishing,
        .entryState = &ecs::villager::EnterFishing,
        .exitState = &ecs::villager::ExitFishing,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FISHING (clip 262 P_FISHERMAN) */
    VillagerStateTableEntry {
        .state = &ecs::villager::Fishing,
        .entryState = &ecs::villager::EnterFishing,
        .exitState = &ecs::villager::ExitFishing,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // no entry or exit: the states that park a villager for a while (the amazed villager of VillagerShield.cpp) need
    // it to come back to their final state
    /* WAIT_FOR_COUNTER */ {.state = &ecs::villager::WaitForCounter},
    // the worship states (VillagerWorship.cpp). GotoWorshipSiteForWorship sets off on the walk to the site's gate
    // (MOVE_ON_PATH or MOVE_TO_POS with 59 to follow), so 58 is only entered when a reaction's state is popped
    // (PopFromPrevious resumes 59 as 58), where its own state function, GotoWorshipSiteForWorship, sets off again.
    // (their exits keep the old convention, false = it may leave: OldExit adapts them)
    /* GOTO_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::GotoWorshipSiteForWorshipState,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* ARRIVES_AT_WORSHIP_SITE_FOR_WORSHIP */
    {.state = &ecs::villager_worship::ArrivesAtWorshipSiteForWorship,
     .exitState = [](LivingAction& a,
                     VillagerStates n) { return OldExit(ecs::villager_worship::ExitMoveToWorshipSite(a, n)); }},
    /* WORSHIPPING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::WorshippingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    // the altar rest (VillagerWorship.cpp; no exits)
    /* GOTO_ALTAR_FOR_REST */ {.state = &ecs::villager_worship::GotoAltarForRest},
    /* ARRIVES_AT_ALTAR_FOR_REST */ {.state = &ecs::villager_worship::ArrivesAtAltarForRest},
    /* AT_ALTAR_REST */ {.state = &ecs::villager_worship::AtAltarRest},
    /* AT_ALTAR_FINISHED_REST */ {.state = &ecs::villager_worship::AtAltarFinishedRest},
    /* RESTART_WORSHIPPING_AT_WORSHIP_SITE (after eating at the site; the exit of 241) */
    {.state = &ecs::villager_worship::RestartWorshippingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitGetFoodAtWorship(a, n)); }},
    // back to a creature's dance after a reaction (VillagerCreatureDance.cpp), its walk to its place through MOVE_TO_POS's
    // step; no entry, exit or validate. (pending) its dance clip
    /* RESTART_WORSHIPPING_CREATURE */
    {.state = [](LivingAction& action) -> uint32_t {
	    return ecs::villager_creature_dance::RestartWorshippingCreature(action, &VillagerMoveToPos);
    }},
    // the farmers (VillagerFarmer.cpp): entry EnterFarming (0 = refused -> 163), exit ExitFarming; always reacts to
    // the town emergency
    /* FARMER_ARRIVES_AT_FARM (clip 259 P_FARMER_SOWING_SEEDS) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerArrivesAtFarm,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FARMER_PLANTS_CROP (clip 259) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerPlantsCrop,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FARMER_DIGS_UP_CROP (clip 257 P_FARMER_HARVESTING) */
    VillagerStateTableEntry {
        .state = &ecs::villager::FarmerDigsUpCrop,
        .entryState = &ecs::villager::EnterFarming,
        .exitState = &ecs::villager::ExitFarming,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* MOVE_TO_FOOTBALL_PITCH_CONSTRUCTION */ k_TodoEntry,
    /* FOOTBALL_WALK_TO_POSITION */ TodoWalk(),
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
    /* CREATED (no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::VillagerCreated,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* ARRIVES_IN_ABODE_TO_TRADE */ k_TodoEntry,
    /* ARRIVES_IN_ABODE_TO_PICK_UP_EXCESS */ k_TodoEntry,
    /* MAKE_SCARED_STIFF */ k_TodoEntry,
    /* SCARED_STIFF */ k_TodoEntry,
    // dancing and worshipping in a creature's dance (VillagerCreatureDance.cpp), its walk to its place through
    // MOVE_TO_POS's step; the dances' shared exit; save / load not ported. (pending) its dance clip
    /* WORSHIPPING_CREATURE */
    {.state = [](LivingAction& action) -> uint32_t {
	     return ecs::villager_creature_dance::WorshippingCreature(action, &VillagerMoveToPos);
     },
     .exitState = &ecs::villager_artefact_dance::ExitDance,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
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
    /* HOUSEWIFE_STARTS_GIVING_BIRTH (VillagerBirth.cpp; no entry or exit; (inferred) no code sets 110: the woman's
       special turn calls the function itself) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeStartsGivingBirthState,
    },
    /* HOUSEWIFE_GIVING_BIRTH (VillagerBirth.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeGivingBirthState,
    },
    /* HOUSEWIFE_GIVEN_BIRTH (VillagerBirth.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HousewifeGivenBirth,
    },
    /* CHILD_AT_CRECHE (VillagerChild.cpp; no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildAtCreche,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* CHILD_FOLLOWS_MOTHER (VillagerDecide.cpp; no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildFollowsMother,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* CHILD_BECOMES_ADULT (VillagerAge.cpp; (inferred) only a script sets 115) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ChildBecomesAdultState,
    },
    /* SITS_DOWN_TO_DINNER */ k_TodoEntry,
    /* EAT_FOOD (VillagerFood.cpp; clip 254 EatDinner); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFood,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* EAT_FOOD_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::EatFoodAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* GOTO_BED_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoBedAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* SLEEPING_AT_HOME (clip -4) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepingAtHome,
        .exitState = &ecs::villager::ExitAtHome,
    },
    /* WAKE_UP_AT_HOME = GO_HOME (no code sets 121); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::WakeUpAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // love out of doors (VillagerBreeding.cpp): the entry only accepts, so none; the shared exit; no save or load;
    // always react to the town emergency
    /* START_HAVING_SEX (validate SexValidate) */
    VillagerStateTableEntry {
        .state = &ecs::villager::StartHavingSex,
        .exitState = &ecs::villager::ExitSex,
        .field0x50 = k_TodoEntry.field0x50,
        .validate = &ecs::villager::SexValidate,
    },
    /* HAVING_SEX (clip KISSING, ECS/VillagerAnimation.cpp) */
    VillagerStateTableEntry {
        .state = &ecs::villager::HavingSex,
        .exitState = &ecs::villager::ExitSex,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* STOP_HAVING_SEX */
    VillagerStateTableEntry {
        .state = &ecs::villager::StopHavingSex,
        .exitState = &ecs::villager::ExitSex,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the same at home: nothing sets them and they do nothing; the at-home exit
    /* START_HAVING_SEX_AT_HOME */
    VillagerStateTableEntry {
        .state = &ecs::villager::SexAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* HAVING_SEX_AT_HOME */
    VillagerStateTableEntry {
        .state = &ecs::villager::SexAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* STOP_HAVING_SEX_AT_HOME */
    VillagerStateTableEntry {
        .state = &ecs::villager::SexAtHome,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* WAIT_FOR_DINNER */ k_TodoEntry,
    /* HOMELESS_START (VillagerHome.cpp); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::HomelessStart,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* VAGRANT_START: always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::VagrantStartState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* MORN_DEATH = GO_HOME (no entry or exit; the mourning into / out-of clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeState,
    },
    /* PERFORM_INSPECTION_REACTION */ k_TodoEntry,
    /* APPROACH_OBJECT_REACTION */ k_TodoEntry,
    /* INITIALISE_TELL_OTHERS_ABOUT_OBJECT */ k_TodoEntry,
    /* TELL_OTHERS_ABOUT_INTERESTING_OBJECT */ k_TodoEntry,
    /* APPROACH_VILLAGER_TO_TALK_TO */ k_TodoEntry,
    /* TELL_PARTICULAR_VILLAGER_ABOUT_OBJECT */ k_TodoEntry,
    /* INITIALISE_LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* LOOK_AROUND_FOR_VILLAGER_TO_TELL */ k_TodoEntry,
    /* MOVE_TOWARDS_OBJECT_TO_LOOK_AT */ TodoWalk(),
    /* INITIALISE_IMPRESSED_REACTION */ k_TodoEntry,
    /* PERFORM_IMPRESSED_REACTION */ k_TodoEntry,
    /* INITIALISE_FIGHT_REACTION */ k_TodoEntry,
    /* PERFORM_FIGHT_REACTION */ k_TodoEntry,
    /* HOMELESS_EAT_DINNER */ k_TodoEntry,
    // a creature about (VillagerCreature.cpp): the shared ExitReaction; validate ReactionValidate (VillagerCallValidate);
    // save / load not ported
    /* INSPECT_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::InspectCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PERFORM_INSPECT_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::PerformInspectCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* APPROACH_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::ApproachCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* INITIALISE_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* PERFORM_BEWILDERED_BY_MAGIC_TREE_REACTION */ k_TodoEntry,
    /* TURN_TO_FACE_MAGIC_TREE */ k_TodoEntry,
    /* LOOK_AT_MAGIC_TREE */ k_TodoEntry,
    /* DANCE_FOR_EDITING_PURPOSES */ TodoWalk(),
    // walking to a place in a dance (VillagerDance.cpp): no entry or exit; the plain walks' validate
    // (VillagerCallValidate); always reacts to the town emergency; save / load not ported
    /* MOVE_TO_DANCE_POS */
    {.state = [](LivingAction& action) -> uint32_t { return ecs::villager_dance::MoveToDancePos(action, &VillagerMoveToPos); },
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState,
     .field0x50 = k_TodoEntry.field0x50},
    /* INITIALISE_RESPECT_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::InitialiseRespectCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PERFORM_RESPECT_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::PerformRespectCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FINISH_RESPECT_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::FinishRespectCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* APPROACH_HAND_REACTION */ k_TodoEntry,
    /* FLEEING_FROM_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::FleeingFromCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* TURN_TO_FACE_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::TurnToFaceCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* WATCH_FLYING_OBJECT_REACTION */ k_TodoEntry,
    // pointing at a thing thrown from the hand (VillagerReactions.cpp): the shared ExitReaction; validate
    // ReactionValidate (VillagerCallValidate)
    /* POINT_AT_FLYING_OBJECT_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_reactions::PointAtFlyingObject,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* DECIDE_WHAT_TO_DO (VillagerDecide.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::DecideWhatToDo,
    },
    /* INTERACT_DECIDE_WHAT_TO_DO */ k_TodoEntry,
    /* EAT_OUTSIDE */ k_TodoEntry,
    /* RUN_AWAY_FROM_OBJECT_REACTION */ k_TodoEntry,
    // the walk up to a creature (VillagerCreature.cpp): the shared ExitReaction; validate the wall hug's (nothing), then
    // ReactionValidate
    /* MOVE_TOWARDS_CREATURE_REACTION */
    VillagerStateTableEntry {
        .state = &ecs::villager_creature::MoveTowardsCreatureReaction,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
        .validate = &ecs::villager_creature::MoveTowardsCreatureValidate,
    },
    // the shield's state (VillagerShield.cpp); its exit is the shared ExitReaction
    /* AMAZED_BY_MAGIC_SHIELD_REACTION */
    {.state = &ecs::villager_shield::AmazedByMagicShieldReaction, .exitState = &ecs::villager_reactions::ExitReaction},
    /* VILLAGER_GOSSIPS */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ANIMAL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_WORSHIP_SITE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ABODE (pending: VillagerInteract.h) */
    k_TodoEntry,
    // the disciple's inspection (VillagerInteract.cpp): no entry / exit; always reacts to the town emergency
    /* CHECK_INTERACT_WITH_FIELD */
    {.state = &ecs::villager::CheckInteractWithField},
    /* CHECK_INTERACT_WITH_FISH_FARM */
    {.state = &ecs::villager::CheckInteractWithFishFarm},
    /* CHECK_INTERACT_WITH_TREE */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_BALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_POT */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_FOOTBALL */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_VILLAGER */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_MAGIC_LIVING */ k_TodoEntry,
    /* CHECK_INTERACT_WITH_ROCK */ k_TodoEntry,
    /* ARRIVES_AT_ROCK_FOR_WOOD */ k_TodoEntry,
    /* GOT_WOOD_FROM_ROCK */ k_TodoEntry,
    /* REENTER_BUILDING_STATE (also the "after" state of the building rows: a builder resumes here after a
       reaction) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ReenterBuildingState,
        .entryState = &ecs::villager::EnterBuilding,
        .exitState = &ecs::villager::ExitBuilding,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game: the clear-area check finds no pushable object
    /* ARRIVE_AT_PUSH_OBJECT */ k_TodoEntry,
    // (VillagerForester.cpp): reached only from the interaction tables; no entry, no exit; always reacts to the
    // town emergency
    /* TAKE_WOOD_FROM_TREE (the foresting clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager::TakeWoodFromTree,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* TAKE_WOOD_FROM_POT (always 1) */
    VillagerStateTableEntry {
        .state = &ecs::villager::TakeWoodFromPot,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) 188 / 189 never entered in the shipped game
    /* TAKE_WOOD_FROM_TREE_FOR_BUILDING */ k_TodoEntry,
    /* TAKE_WOOD_FROM_POT_FOR_BUILDING */ k_TodoEntry,
    /* SHEPHERD_TAKE_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* SHEPHERD_TAKES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* SHEPHERD_RELEASES_CONTROL_OF_FLOCK */ k_TodoEntry,
    /* DANCE_BUT_NOT_WORSHIP */ TodoWalk(),
    /* FAINTING_REACTION */ k_TodoEntry,
    /* START_CONFUSED_REACTION */ k_TodoEntry,
    /* CONFUSED_REACTION */ k_TodoEntry,
    /* AFTER_TAP_ON_ABODE (no entry / exit; the clip 273, a yawn, is villager_animation's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::AfterTapOnAbode,
    },
    /* WEAK_ON_GROUND */ k_TodoEntry,
    // wandering round the script's place (villager_script, VillagerScript.h), in and out as IN_SCRIPT; save / load not
    // ported
    /* SCRIPT_WANDER_AROUND_POSITION */
    {.state = &ecs::villager_script::ScriptWanderAroundPosition,
     .entryState = &ecs::villager_script::EnterInScript,
     .exitState = &ecs::villager_script::ExitInScript,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the script's clip (villager_animation's ClipChoice::Script); save / load not ported
    /* SCRIPT_PLAY_ANIM */
    {.state = &ecs::villager_script::ScriptPlayAnim,
     .entryState = &ecs::villager_script::EnterPlayAnim,
     .exitState = &ecs::villager_script::ExitPlayAnim,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the teleport stones' states (VillagerTeleport.cpp); their exit ExitReactToTeleport
    /* GO_TOWARDS_TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* TELEPORT_REACTION */
    {.state = &ecs::villager_teleport::TeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* DANCE_WHILE_REACTING */ TodoWalk(TodoWithExitReaction()),
    // called to a creature's dance (VillagerCreatureDance.cpp): its exit lets it go only into the dance, a decision, the
    // hand, a flight or a death. (pending) its clip
    /* CONTROLLED_BY_CREATURE */
    {.state = &ecs::villager_creature_dance::ControlledByCreature,
     .exitState = &ecs::villager_creature_dance::ExitControlledByCreature},
    // the mourning (ECS/Villager/VillagerMourning.cpp): exit ExitReaction, validate ReactionValidate, both for
    // REACT_TO_DEATH's reaction
    /* POINT_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::PointAtDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* GO_TOWARDS_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::GoTowardsDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* LOOK_AT_DEAD_PERSON */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::LookAtDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* MOURN_DEAD_PERSON (the mourning into / out-of clip) */
    VillagerStateTableEntry {
        .state = &ecs::villager_mourning::MournDeadPerson,
        .exitState = &ecs::villager_mourning::ExitReaction,
        .validate = &ecs::villager_mourning::ReactionValidate,
    },
    /* NOTHING_TO_DO (no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::NothingToDo,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // the craftsman's states (VillagerSatisfy.cpp): no entry, exit or validate; always react to the town emergency
    /* ARRIVES_AT_WORKSHOP_FOR_DROP_OFF */ {.state = &ecs::villager::ArrivesAtWorkshopForDropOff},
    /* ARRIVES_AT_STORAGE_PIT_FOR_WORKSHOP_MATERIALS */ {.state = &ecs::villager::ArrivesAtStoragePitForWorkshopMaterials},
    /* SHOW_POISONED (VillagerFood.cpp; clip 342) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ShowPoisoned,
    },
    /* HIDING_AT_WORSHIP_SITE */
    {.state = &ecs::villager_worship::HidingAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitAtWorshipSite(a, n)); }},
    /* CROWD_REACTION */ TodoWithExitReaction(),
    // the fire's states (VillagerFire.cpp): 216, 217, 218 and 220 enter and exit with EnterPutOutFire /
    // ExitPutOutFire, 219 with EnterOnFire / ExitOnFire. 215's
    // exit is the shared ExitReaction
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
    /* DISCIPLE_NOTHING_TO_DO, entry EnterDiscipleNothingToDo (VillagerDisciple.cpp; no exit); save / load not ported
       (k_TodoEntry's); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::DiscipleNothingToDo,
        .entryState = &ecs::villager::EnterDiscipleNothingToDo,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* FOOTBALL_MOVE_TO_BALL */ TodoWalkWithExitMoveToPos(),
    // the trader's states (VillagerTrader.cpp): no entry, exit or validate; save / load not ported; always react to
    // the town emergency
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_PICK_UP */
    {.state = &ecs::villager::ArrivesAtStoragePitForTraderPickUp,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* ARRIVES_AT_STORAGE_PIT_FOR_TRADER_DROP_OFF */
    {.state = &ecs::villager::ArrivesAtStoragePitForTraderDropOff,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    // the breeder disciple (VillagerBreeding.cpp): its call made on entry and ended on exit; save / load not ported;
    // validate SexValidate; always reacts to the town emergency
    /* BREEDER_DISCIPLE */
    VillagerStateTableEntry {
        .state = &ecs::villager::BreederDisciple,
        .entryState = &ecs::villager::EnterBreeder,
        .exitState = &ecs::villager::ExitBreeder,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
        .validate = &ecs::villager::SexValidate,
    },
    // the missionary's state (VillagerMissionary.cpp): no entry, exit or validate; always reacts to the town emergency
    /* MISSIONARY_DISCIPLE */ {.state = &ecs::villager::MissionaryDisciple},
    /* REACT_TO_BREEDER (VillagerBreeding.cpp): the reaction rows' exit (and their validate, through the original's
       column); always reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::ReactToBreeder,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* SHEPHERD_CHECK_ANIMAL_FOR_SLAUGHTER */ k_TodoEntry,
    /* INTERACT_DECIDE_WHAT_TO_DO_FOR_OTHER_VILLAGER */ k_TodoEntry,
    // dancing round an artefact (VillagerArtefactDance.cpp), its walk to its place through MOVE_TO_POS's step; the
    // dances' shared exit; save / load not ported
    /* ARTIFACT_DANCE */
    {.state = [](LivingAction& action) -> uint32_t {
	     return ecs::villager_artefact_dance::ArtefactDance(action, &VillagerMoveToPos);
     },
     .exitState = &ecs::villager_artefact_dance::ExitDance,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* FLEEING_FROM_PREDATOR_REACTION (VillagerReactions.cpp): the shared ExitReaction; validate ReactionValidate
       (VillagerCallValidate); save / load not ported */
    VillagerStateTableEntry {
        .state = &ecs::villager_reactions::FleeingFromPredator,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    // (not ported) never entered in the shipped game: SetupWaitForWood needs a game flag that is never set
    /* WAIT_FOR_WOOD */ k_TodoEntry,
    /* INSPECT_OBJECT (VillagerDisciple.cpp): validate ClosestObjectValidate; save / load not ported; always reacts to
       the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::InspectObject,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .validate = &ecs::villager::ClosestObjectValidate,
    },
    /* GO_HOME_AND_CHANGE, exit ExitGoHomeAndChange (VillagerHome.cpp: the grown-up child's adult mesh); always reacts
       to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoHomeAndChange,
        .exitState = &ecs::villager::ExitGoHomeAndChange,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* WAIT_FOR_MATE */ k_TodoEntry,
    /* GO_AND_HIDE_IN_NEARBY_BUILDING and LOOK_TO_SEE_IF_IT_IS_SAFE (VillagerCover.cpp): the shared ExitReaction;
       validate ReactionValidate (VillagerCallValidate); save / load not ported; always react to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager_cover::GoAndHideInNearbyBuilding,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    VillagerStateTableEntry {
        .state = &ecs::villager_cover::LookToSeeIfItIsSafe,
        .exitState = &ecs::villager_reactions::ExitReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* SLEEP_IN_TENT (VillagerHome.cpp; clip 381 and the tent's in / out clip of villager_animation); always
       reacts to the town emergency */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PAUSE_FOR_A_SECOND (no entry, exit, save or load; always reacts to the town emergency; its clip function is
       villager_animation's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::PauseForASecond,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* PANIC_REACTION (VillagerCover.cpp; no entry, exit, save or load; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager_cover::PanicReaction,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* GET_FOOD_AT_WORSHIP_SITE (VillagerWorship.cpp: a meal from the site's food pot, then 65) */
    {.state = &ecs::villager_worship::GetFoodAtWorshipSite,
     .exitState = [](LivingAction& a, VillagerStates n) { return OldExit(ecs::villager_worship::ExitGetFoodAtWorship(a, n)); }},
    /* GOTO_CONGREGATE_IN_TOWN_AFTER_EMERGENCY (no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GotoCongregateInTownAfterEmergency,
    },
    /* CONGREGATE_IN_TOWN_AFTER_EMERGENCY (no entry / exit; the town emergency clip is villager_animation's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::CongregateInTownAfterEmergency,
    },
    /* SCRIPT_IN_CROWD (VillagerScript.cpp; no entry or exit) */
    VillagerStateTableEntry {
        .state = &ecs::villager::ScriptInCrowd,
        .saveState = k_TodoEntry.saveState,
        .loadState = k_TodoEntry.loadState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* GO_AND_CHILLOUT_OUTSIDE_HOME (VillagerDecide.cpp) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutOutsideHome,
    },
    /* SIT_AND_CHILLOUT, entry EnterSitAndChillOut (its sit-down clip functions are villager_animation's) */
    VillagerStateTableEntry {
        .state = &ecs::villager::SitAndChillout,
        .entryState = &ecs::villager::EnterSitAndChillOut,
    },
    /* SCRIPT_GO_AND_MOVE_ALONG_PATH */ k_TodoEntry,
    // (k_VillagerStateStrings mislabels 248..254; the names here are the enum's / info.dat's)
    /* 248 GO_HOME_FROM_WORSHIP = DoGoingHome(249, 250) (VillagerHome.cpp); exit ExitAtHome; always reacts to the
       town emergency */
    VillagerStateTableEntry {
        .state = [](LivingAction& action) -> uint32_t {
	        return ecs::villager::DoGoingHome(Locator::entitiesRegistry::value().ToEntity(action),
	                                          VillagerStates::ArrivesHomeFromWorship, VillagerStates::SleepInTentFromWorship);
        },
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 249 ARRIVES_HOME_FROM_WORSHIP = ARRIVES_HOME; exit ExitAtHome */
    VillagerStateTableEntry {
        .state = &ecs::villager::ArrivesHomeState,
        .exitState = &ecs::villager::ExitAtHome,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 250 SLEEP_IN_TENT_FROM_WORSHIP = SLEEP_IN_TENT */
    VillagerStateTableEntry {
        .state = &ecs::villager::SleepInTentState,
        .field0x50 = k_TodoEntry.field0x50,
    },
    /* 251 GO_TOWARDS_TELEPORT_REACTION_QUICKLY (= 201's; exit ExitReactToTeleport) */
    {.state = &ecs::villager_teleport::GoToTeleportReaction, .exitState = &ecs::villager_teleport::ExitReactToTeleport},
    /* 252 GO_AND_CHILLOUT_IN_TOWN (VillagerDecide.cpp; only scripts set it) */
    VillagerStateTableEntry {
        .state = &ecs::villager::GoAndChilloutInTown,
    },
    /* 253 WAIT_FOR_ARTIFACT_DANCE (VillagerArtefactDance.cpp); the dances' shared exit; save / load not ported */
    {.state = &ecs::villager_artefact_dance::WaitForArtefactDance,
     .entryState = &ecs::villager_artefact_dance::EnterWaitForArtefactDance,
     .exitState = &ecs::villager_artefact_dance::ExitDance,
     .saveState = k_TodoEntry.saveState,
     .loadState = k_TodoEntry.loadState},
    /* 254 BREEDER_JUST_LANDED (VillagerBreeding.cpp; no entry or exit; always reacts to the town emergency) */
    VillagerStateTableEntry {
        .state = &ecs::villager::BreederJustLanded,
        .field0x50 = k_TodoEntry.field0x50,
    },
};

void LivingActionSystem::Update()
{
	// the villagers' turn is in the one living list with the animals (living_turn::ProcessLiving, ECS/LivingTurn.h); this
	// is what follows it: the souls of the dead, (approximate) once a game turn with its ms
	// (game_clock::MsPerTurn); the original runs it every frame with the frame's time step
	ecs::villager_soul::Update(game_clock::MsPerTurn());
}

VillagerStates LivingActionSystem::VillagerGetState(const LivingAction& action, LivingAction::Index index) const
{
	return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
}

VillagerStates LivingActionSystem::VillagerGetFinalState(const LivingAction& action) const
{
	return ecs::villager::GetFinalState(Locator::entitiesRegistry::value().ToEntity(action));
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
		// the PREVIOUS rule and the town modifier
		ecs::villager::SetState(entity, index, state);
		return;
	}
	if (!skipTransition)
	{
		// the pause, the exits, the entry, the refusals, the speed and the clips
		ecs::villager::SetTopState(entity, state);
		return;
	}
	// The changes that bypass the exit and entry functions (the physics' landing, the animals, the water, MOVE_TO_POS
	// and LANDED): the state is set straight (FINAL cleared with its town modifier, the counter 0) and the clips and speed
	// as before. (approximate) in the original those go through SetTopState with their own entry / exit functions,
	// not ported yet. Repeating the same order changes nothing, as
	// openblack had it
	if (previousState == state)
	{
		return;
	}
	// leaving the hand this way runs the hand's exit too, as the original's change of the top state does there, and
	// stays in the hand when the exit refuses
	if (previousState == VillagerStates::InHand && VillagerCallExitState(action, previousState, state) == 0)
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
	const auto index = std::min<size_t>(static_cast<size_t>(row), 254);
	const auto& original = ecs::villager::k_StateFunctionIds.at(index);
	const uint8_t function = slot == Slot::Entry ? original.entry : slot == Slot::Exit ? original.exit : original.validate;
	if (function == 0 || !log_once::First(fmt::format("ai/unported-slot/{}/{}", static_cast<int>(slot), index)))
	{
		return;
	}
	SPDLOG_LOGGER_WARN(spdlog::get("ai"), "Villager #{}: TODO: Unimplemented {} function of {} {}: taken as 1",
	                   static_cast<uint32_t>(Locator::entitiesRegistry::value().ToEntity(action)),
	                   slot == Slot::Entry  ? "entry"
	                   : slot == Slot::Exit ? "exit"
	                                        : "validate",
	                   index, k_VillagerStateStrings.at(index));
}
} // namespace

uint32_t LivingActionSystem::VillagerCallEntryState(LivingAction& action, VillagerStates row, VillagerStates final,
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

uint32_t LivingActionSystem::VillagerCallExitState(LivingAction& action, VillagerStates row, VillagerStates next) const
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
	const auto original = ecs::villager::k_StateFunctionIds.at(std::min<size_t>(static_cast<size_t>(state), 254)).validate;
	// the reaction rows' validate is ReactionValidate (201, 202, 251, 215-218,
	// 220, 6-30, 140-196...: every row whose original validate is it, VillagerStateFunctions.h). The villager's state turn
	// calls this slot for its two states before the state function; the result is unused there
	if (!callback && original == ecs::villager::k_ReactionValidate)
	{
		return ecs::villager_reactions::ReactionValidate(action);
	}
	// the plain walks' validate (MOVE_TO_POS, the forester's walk to the forest, the football walk to a position and the
	// walk to a dance place) checks nothing
	if (!callback && original == ecs::villager::k_WallHugValidate)
	{
		return true;
	}
	if (!callback)
	{
		WarnMissing(action, state, Slot::Validate);
		return false;
	}
	return callback(action);
}

bool LivingActionSystem::VillagerSetCurrentAndDestinationState(LivingAction& action, VillagerStates current,
                                                               VillagerStates destination) const
{
	return ecs::villager::SetCurrentAndDestinationState(Locator::entitiesRegistry::value().ToEntity(action), current,
	                                                    destination) == ecs::villager::k_Done;
}

void LivingActionSystem::VillagerPlayAnimThenSetState(LivingAction& action, VillagerStates next) const
{
	ecs::villager::PlayAnimThenSetState(Locator::entitiesRegistry::value().ToEntity(action), next);
}

void LivingActionSystem::VillagerSetTopStateToFinal(LivingAction& action) const
{
	ecs::villager::SetTopStateToFinal(Locator::entitiesRegistry::value().ToEntity(action));
}

bool LivingActionSystem::VillagerIsReadyForNewAnimation(const LivingAction& action, uint32_t times) const
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* animation = registry.TryGet<const SkeletalAnimation>(registry.ToEntity(action));
	if (animation == nullptr || !animation->hasClip)
	{
		return true;
	}
	return ecs::villager_clips::ClipPlayed(action, animation->clip, times);
}

bool LivingActionSystem::VillagerCanBeDirected(entt::entity villager) const
{
	return ecs::villager_script::CanBeDirected(villager);
}

void LivingActionSystem::VillagerSetScriptState(entt::entity villager, VillagerStates state) const
{
	ecs::villager_script::SetScriptState(villager, state);
}

void LivingActionSystem::VillagerScriptMoveTo(entt::entity villager, glm::vec2 goal) const
{
	ecs::villager_script::MoveTo(villager, goal);
}

void LivingActionSystem::VillagerSetScriptAnimation(entt::entity villager, AnimId clip, uint32_t plays) const
{
	ecs::villager_script::SetScriptAnimation(villager, static_cast<uint32_t>(clip), plays);
}

void LivingActionSystem::VillagerSetScriptPlace(entt::entity villager, glm::vec2 place) const
{
	ecs::villager_script::SetScriptPlace(villager, place);
}

void LivingActionSystem::VillagerSetScriptNumber(entt::entity villager, float number) const
{
	ecs::villager_script::SetScriptNumber(villager, number);
}

bool LivingActionSystem::VillagerHasPlayedScriptAnimation(entt::entity villager) const
{
	return ecs::villager_script::HasPlayedScriptAnimation(villager);
}

void LivingActionSystem::VillagerFace(entt::entity villager, glm::vec2 point) const
{
	// the angle reads only x and z
	ecs::living::SetFocus(villager, glm::vec3(point.x, 0.0f, point.y));
}

std::optional<float> LivingActionSystem::VillagerYAngle(entt::entity villager) const
{
	const auto* wallHug = Locator::entitiesRegistry::value().TryGet<const WallHug>(villager);
	return wallHug != nullptr ? std::optional(wallHug->yAngle) : std::nullopt;
}

void LivingActionSystem::VillagerSetYAngle(entt::entity villager, float angle) const
{
	ecs::villager::SetYAngle(villager, angle);
}

void LivingActionSystem::VillagerOverrideAnimation(entt::entity villager, int32_t clip) const
{
	ecs::villager_animate::OverrideAnim(villager, clip);
}

void LivingActionSystem::VillagerSetAge(entt::entity villager, uint32_t age) const
{
	ecs::villager::SetAgeAndScale(villager, age);
}
