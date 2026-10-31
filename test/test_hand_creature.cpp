/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// hand_creature: the hand's locked select on a creature, its time with it, the click and the feedback, frame by frame
// with fake facts. Without a creature the hand's state follows the required state as before

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "ECS/HandCreature.h"
#include "ECS/ObjectFlags.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack::ecs::hand_creature;

namespace
{
constexpr int32_t k_Normal = 1;

LockFacts Facts(bool locked, bool actionReleased = false)
{
	return {.available = true, .locked = locked, .stillSelected = true, .actionReleased = actionReleased};
}

std::vector<Send> SendsOf(const FrameStep& step)
{
	return {step.Sends().begin(), step.Sends().end()};
}

/// A frame of a creature held in the hand, the interface in Creature Interaction
FrameFacts Holding(uint32_t cameraMs, int32_t renderState = k_CreatureHandState)
{
	return {
	    .creatureInteraction = true,
	    .hasCreature = true,
	    .cameraMs = cameraMs,
	    .requiredState = k_CreatureHandState,
	    .renderState = renderState,
	    .creatureLocked = true,
	};
}

/// The first frame after the interface left Creature Interaction
FrameFacts LettingGo(uint32_t cameraMs)
{
	return {
	    .creatureInteraction = false,
	    .hasCreature = true,
	    .cameraMs = cameraMs,
	    .requiredState = k_Normal,
	    .renderState = k_CreatureHandState,
	    .creatureLocked = true,
	};
}

const auto k_Own = []() { return true; };
const auto k_NotOwn = []() { return false; };
} // namespace

TEST(HandCreatureLock, WaitsForTheTurnToLockThenHolds)
{
	const auto waiting = StepLock({}, Facts(false));
	EXPECT_EQ(waiting.next.phase, LockPhase::Waiting);
	EXPECT_FALSE(waiting.sendEnd);
	EXPECT_FALSE(waiting.letGo);
	const auto held = StepLock(waiting.next, Facts(true));
	EXPECT_EQ(held.next.phase, LockPhase::Held);
	EXPECT_FALSE(held.sendEnd);
	EXPECT_FALSE(held.letGo);
}

TEST(HandCreatureLock, ALockTheTurnRefusedIsLetGo)
{
	const auto step = StepLock({}, {.available = true, .locked = false, .stillSelected = false});
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, TheReleaseEndsItOneFrameLaterAndTheAppliedEndLetsGo)
{
	LockState state {.phase = LockPhase::Held};
	auto step = StepLock(state, Facts(true, true));
	EXPECT_EQ(step.next.phase, LockPhase::WaitingForLockOff);
	EXPECT_FALSE(step.sendEnd);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Completion);
	EXPECT_TRUE(step.sendEnd);
	EXPECT_FALSE(step.letGo);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Completion);
	EXPECT_FALSE(step.sendEnd);
	EXPECT_FALSE(step.letGo);
	step = StepLock(step.next, Facts(false));
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, AReleaseBeforeTheLockIsKeptUntilHeld)
{
	// the button goes up while the turn has not locked the creature yet: the held phase still reads it
	auto step = StepLock({}, Facts(false, true));
	EXPECT_EQ(step.next.phase, LockPhase::Waiting);
	EXPECT_TRUE(step.next.released);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Held);
	step = StepLock(step.next, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::WaitingForLockOff);
}

TEST(HandCreatureLock, ALockWithoutAGrabKeepsTheSelectUntilTheRelease)
{
	// Pins the lock's steps for a creature the turn locked though the hand could not take hold of it. The steps
	// never ask whether it is held, so the select is neither ended nor let go while the button is down, and ends one
	// frame after the release as for a held creature. The turn's start itself has no harness here, so this is not a
	// test of how the turn locks it
	auto step = StepLock({}, Facts(true));
	EXPECT_EQ(step.next.phase, LockPhase::Held);
	for (int frame = 0; frame < 3; ++frame)
	{
		step = StepLock(step.next, Facts(true));
		EXPECT_EQ(step.next.phase, LockPhase::Held) << frame;
		EXPECT_FALSE(step.sendEnd) << frame;
		EXPECT_FALSE(step.letGo) << frame;
	}
	step = StepLock(step.next, Facts(true, true));
	EXPECT_EQ(step.next.phase, LockPhase::WaitingForLockOff);
	EXPECT_FALSE(step.sendEnd);
	step = StepLock(step.next, Facts(true));
	EXPECT_TRUE(step.sendEnd);
	EXPECT_FALSE(step.letGo);
}

TEST(HandCreatureLock, AHeldCreatureNoLongerLockedIsLetGo)
{
	const auto step = StepLock({.phase = LockPhase::Held}, Facts(false));
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.sendEnd);
}

TEST(HandCreatureLock, ACreatureGoneEndsItUnlessTheEndIsSent)
{
	for (const auto phase : {LockPhase::Waiting, LockPhase::Held, LockPhase::WaitingForLockOff})
	{
		const auto step = StepLock({.phase = phase}, {.available = false, .locked = true, .stillSelected = true});
		EXPECT_TRUE(step.sendEnd) << static_cast<int>(phase);
		EXPECT_TRUE(step.letGo) << static_cast<int>(phase);
	}
	const auto step = StepLock({.phase = LockPhase::Completion}, {.available = false, .locked = true});
	EXPECT_FALSE(step.sendEnd);
	EXPECT_TRUE(step.letGo);
}

TEST(HandLockedObject, TheTurnKeepsAnObjectStillThereInTheInfluence)
{
	for (const bool pickSource : {false, true})
	{
		const auto step = StepLockedObject({.available = true, .influence = 0.25f, .pickSource = pickSource});
		EXPECT_FALSE(step.letGo) << pickSource;
		EXPECT_FALSE(step.endAction) << pickSource;
	}
}

TEST(HandLockedObject, AGoneCreatureOrTotemIsLetGoAndTheActionEnds)
{
	for (const bool creature : {false, true})
	{
		const auto step = StepLockedObject({.available = false, .creature = creature, .influence = 1.0f});
		EXPECT_TRUE(step.letGo) << creature;
		EXPECT_TRUE(step.endAction) << creature;
	}
}

TEST(HandLockedObject, AGoneSourceIsLetGoAndItsOwnPassEndsTheAction)
{
	const auto step = StepLockedObject({.available = false, .influence = 1.0f, .pickSource = true});
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.endAction);
}

TEST(HandLockedObject, ATotemOutOfTheInfluenceIsDroppedAndTheActionEnds)
{
	// no influence at the hand's point, or none to spare: the totem is let go of
	for (const float influence : {0.0f, -0.5f})
	{
		const auto step = StepLockedObject({.available = true, .influence = influence});
		EXPECT_TRUE(step.letGo) << influence;
		EXPECT_TRUE(step.endAction) << influence;
	}
	// the least influence above 0 keeps it
	EXPECT_FALSE(StepLockedObject({.available = true, .influence = 1e-6f}).letGo);
}

TEST(HandLockedObject, ACreatureIsKeptOutOfTheInfluence)
{
	const auto step = StepLockedObject({.available = true, .creature = true, .influence = -1.0f});
	EXPECT_FALSE(step.letGo);
	EXPECT_FALSE(step.endAction);
}

TEST(HandLockedObject, ASourceOutOfTheInfluenceIsLetGoAndItsOwnPassEndsTheAction)
{
	const auto step = StepLockedObject({.available = true, .influence = 0.0f, .pickSource = true});
	EXPECT_TRUE(step.letGo);
	EXPECT_FALSE(step.endAction);
}

TEST(HandLockedObject, ASourceThatGaveNothingIsLetGoAndTheActionEndsInItsStartsTurn)
{
	// a field, fish farm or pile whose start took nothing, still there and in the influence: no handful in the hand, so
	// its interaction is over and the turn's pass ends the action itself (the end packet, then EndAction)
	const auto step = StepLockedObject({.available = true, .influence = 0.25f, .interacts = false});
	EXPECT_TRUE(step.letGo);
	EXPECT_TRUE(step.endAction);
	// out of the influence too, the same
	const auto outside = StepLockedObject({.available = true, .influence = 0.0f, .interacts = false});
	EXPECT_TRUE(outside.letGo);
	EXPECT_TRUE(outside.endAction);
}

TEST(HandCreatureLockFlag, ACreaturesStartSetsItAndOnlyACreaturesEndClearsIt)
{
	EXPECT_TRUE(StepCreatureLockFlag(false, LockFlagEvent::CreatureStart));
	EXPECT_TRUE(StepCreatureLockFlag(true, LockFlagEvent::CreatureStart));
	EXPECT_FALSE(StepCreatureLockFlag(true, LockFlagEvent::CreatureEnd));
	EXPECT_FALSE(StepCreatureLockFlag(false, LockFlagEvent::CreatureEnd));
	// another object's start or end leaves it as it was
	EXPECT_TRUE(StepCreatureLockFlag(true, LockFlagEvent::Other));
	EXPECT_FALSE(StepCreatureLockFlag(false, LockFlagEvent::Other));
}

TEST(HandCreatureLockFlag, ACreatureGoneWhileLockedLeavesItSet)
{
	// the creature's start, then the creature gone: the turn lets go of it and ends the action, whose end sends nothing
	// for an object that is gone, so no creature's end is ever applied
	bool flag = StepCreatureLockFlag(false, LockFlagEvent::CreatureStart);
	const auto step = StepLockedObject({.available = false, .creature = true});
	ASSERT_TRUE(step.endAction);
	flag = StepCreatureLockFlag(flag, LockFlagEvent::Other);
	EXPECT_TRUE(flag);
	// until the next creature's select ends
	flag = StepCreatureLockFlag(StepCreatureLockFlag(flag, LockFlagEvent::CreatureStart), LockFlagEvent::CreatureEnd);
	EXPECT_FALSE(flag);
}

TEST(HandReadyForObject, SpaceInTheHandsAndNoCreatureLockedIsAllItAsks)
{
	EXPECT_TRUE(IsHandReadyForObject(true, false));
	// something held, or a creature locked for the hand (a gone one among them), refuses it
	EXPECT_FALSE(IsHandReadyForObject(false, false));
	EXPECT_FALSE(IsHandReadyForObject(true, true));
	EXPECT_FALSE(IsHandReadyForObject(false, true));
	// a creature gone while locked keeps refusing it until a creature's end is applied
	bool flag = StepCreatureLockFlag(false, LockFlagEvent::CreatureStart);
	flag = StepCreatureLockFlag(flag, LockFlagEvent::Other);
	EXPECT_FALSE(IsHandReadyForObject(true, flag));
	EXPECT_TRUE(IsHandReadyForObject(true, StepCreatureLockFlag(flag, LockFlagEvent::CreatureEnd)));
}

TEST(HandCreatureFrame, TheCreatureStateWaitsForTheLock)
{
	auto facts = Holding(16, k_Normal);
	facts.creatureLocked = false;
	EXPECT_EQ(StepFrame({}, facts, k_Own).renderState, k_Normal);
	facts.creatureLocked = true;
	const auto step = StepFrame({}, facts, k_Own);
	EXPECT_EQ(step.renderState, k_CreatureHandState);
	EXPECT_TRUE(step.Sends().empty());
}

TEST(HandCreatureFrame, TheCreatureStateWaitsForTheBodyToFinish)
{
	// locked, the creature still finishes the body action it was doing when taken hold: the hand stays as it is
	auto facts = Holding(16, k_Normal);
	facts.creatureFinishing = true;
	EXPECT_EQ(StepFrame({}, facts, k_Own).renderState, k_Normal);
	facts.creatureFinishing = false;
	EXPECT_EQ(StepFrame({}, facts, k_Own).renderState, k_CreatureHandState);
}

TEST(HandCreatureFrame, ALockedCreatureFinishesTheFirstTurnItsBodyPlaysNothing)
{
	// it starts finishing as it is locked, and stays so while its body plays an action
	EXPECT_TRUE(StillFinishing(true, true, true));
	EXPECT_FALSE(StillFinishing(true, true, false));
	// once done it is done until it is let go, whatever its body plays next (a stroke's or a slap's reaction)
	EXPECT_FALSE(StillFinishing(false, true, true));
	// let go, it no longer finishes anything
	EXPECT_FALSE(StillFinishing(true, false, true));
}

TEST(HandCreatureLock, TheBodyIsTakenOffTheGameTheTurnTheCreatureStateBegins)
{
	// the turn it stops finishing, still locked
	EXPECT_TRUE(TakesBodyOffTheGame(true, false, true));
	// not while it finishes, nor once it is off, nor after the lock is gone
	EXPECT_FALSE(TakesBodyOffTheGame(true, true, true));
	EXPECT_FALSE(TakesBodyOffTheGame(false, false, true));
	EXPECT_FALSE(TakesBodyOffTheGame(true, false, false));
}

TEST(HandCreatureFrame, ALockLostInTheCreatureStateDoesNotHoldIt)
{
	// the lock is needed to enter the state, not to stay in it
	auto facts = Holding(16);
	facts.creatureLocked = false;
	EXPECT_EQ(StepFrame({.started = true}, facts, k_Own).renderState, k_CreatureHandState);
}

TEST(HandCreatureFrame, TheTimeStartsAtTheInteractionsFirstFrame)
{
	auto step = StepFrame({.cameraMs = 5000}, Holding(16), k_Own);
	EXPECT_TRUE(step.interaction.started);
	EXPECT_EQ(step.interaction.cameraMs, 0u);
	step = StepFrame(step.interaction, Holding(16), k_Own);
	EXPECT_EQ(step.interaction.cameraMs, 16u);
}

TEST(HandCreatureFrame, ALetGoWithinAClickOfTheOwnCreatureSendsTheClickThenTheFeedback)
{
	auto step = StepFrame({}, Holding(16), k_Own);
	step = StepFrame(step.interaction, Holding(200), k_Own);
	step = StepFrame(step.interaction, LettingGo(200), k_Own);
	EXPECT_FALSE(step.interaction.started);
	EXPECT_EQ(step.interaction.cameraMs, 400u);
	EXPECT_EQ(step.renderState, k_Normal);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Click, Send::Feedback}));
}

TEST(HandCreatureFrame, ALongHoldSendsOnlyTheFeedback)
{
	auto step = StepFrame({}, Holding(16), k_Own);
	step = StepFrame(step.interaction, Holding(300), k_Own);
	step = StepFrame(step.interaction, LettingGo(150), k_Own);
	EXPECT_EQ(step.interaction.cameraMs, 450u);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Feedback}));
}

TEST(HandCreatureFrame, AnotherPlayersCreatureIsNoClick)
{
	auto step = StepFrame({}, Holding(16), k_NotOwn);
	step = StepFrame(step.interaction, LettingGo(16), k_NotOwn);
	EXPECT_EQ(SendsOf(step), (std::vector<Send> {Send::Feedback}));
}

TEST(HandCreatureFrame, TheOwnerIsAskedOnlyWhenTheInteractionEnds)
{
	int asked = 0;
	const auto own = [&asked]() {
		++asked;
		return true;
	};
	auto step = StepFrame({}, Holding(16), own);
	step = StepFrame(step.interaction, Holding(16), own);
	EXPECT_EQ(asked, 0);
	step = StepFrame(step.interaction, LettingGo(16), own);
	EXPECT_EQ(asked, 1);
	step = StepFrame(step.interaction, {.requiredState = k_Normal, .renderState = k_Normal}, own);
	EXPECT_EQ(asked, 1);
}

TEST(HandCreatureFrame, WithoutACreatureTheStateFollowsTheRequiredOne)
{
	// no creature: no time started, nothing sent, and the hand takes every required state but CREATURE at once
	for (int32_t required = 0; required < 12; ++required)
	{
		for (const int32_t current : {0, 1, 5, 9})
		{
			const auto step = StepFrame({}, {.cameraMs = 16, .requiredState = required, .renderState = current}, k_NotOwn);
			EXPECT_EQ(step.renderState, required == k_CreatureHandState ? current : required) << required << " " << current;
			EXPECT_TRUE(step.Sends().empty());
			EXPECT_FALSE(step.interaction.started);
		}
	}
}

TEST(HandLockedSelect, AStartTakesAnInteractableObjectNotInALockedSelectAlready)
{
	EXPECT_TRUE(StartTakesObject(true, true, false));
	// no object, one that is not interactable, or one in a locked select already: the start only ends the action
	EXPECT_FALSE(StartTakesObject(false, true, false));
	EXPECT_FALSE(StartTakesObject(true, false, false));
	EXPECT_FALSE(StartTakesObject(true, true, true));
}

TEST(HandLockedSelect, TheFlagIsSetByTheStartThatTookTheObjectAndClearedOnlyByItsEnd)
{
	EXPECT_TRUE(StepLockedSelectFlag(false, SelectFlagEvent::StartTaken));
	EXPECT_TRUE(StepLockedSelectFlag(true, SelectFlagEvent::StartTaken));
	// a start refused leaves the flag as it was: one set stays set, one clear stays clear
	EXPECT_TRUE(StepLockedSelectFlag(true, SelectFlagEvent::StartRefused));
	EXPECT_FALSE(StepLockedSelectFlag(false, SelectFlagEvent::StartRefused));
	EXPECT_FALSE(StepLockedSelectFlag(true, SelectFlagEvent::End));
	EXPECT_FALSE(StepLockedSelectFlag(false, SelectFlagEvent::End));
}

TEST(HandLockedSelect, AScoopThatTookNothingStaysInTheSelectUntilItsEndIsApplied)
{
	// The press's turn: the start packet takes the pile, whose own start finds nothing to scoop
	bool flag = false;
	ASSERT_TRUE(StartTakesObject(true, true, flag));
	flag = StepLockedSelectFlag(flag, SelectFlagEvent::StartTaken);
	// the same turn's pass finds no handful in the hand and ends the select, which only sends the end packet: the
	// scripts of that turn still see the pile locked
	EXPECT_TRUE(flag);
	// a second press on it before the end is applied is refused, and leaves it locked
	EXPECT_FALSE(StartTakesObject(true, true, flag));
	flag = StepLockedSelectFlag(flag, SelectFlagEvent::StartRefused);
	EXPECT_TRUE(flag);
	// the next turn applies the end packet: the pile is out of the select
	flag = StepLockedSelectFlag(flag, SelectFlagEvent::End);
	EXPECT_FALSE(flag);
	EXPECT_TRUE(StartTakesObject(true, true, flag));
}

// The flag an object carries while it is in the hand's locked select, as the start and end packets set and clear it
TEST(HandLockedSelect, AnEndForNoObjectOrOneNotInteractableEndsTheAction)
{
	EXPECT_FALSE(EndPacketEndsAction(true, true));
	EXPECT_TRUE(EndPacketEndsAction(true, false));
	EXPECT_TRUE(EndPacketEndsAction(false, false));
}

TEST(HandLockedSelect, AnObjectIsInTheSelectFromItsFlagSetUntilItIsClearedOrTheObjectIsGone)
{
	namespace object_flags = openblack::ecs::object_flags;
	const openblack::test::RestoreService<openblack::Locator::entitiesRegistry> restoreRegistry;
	openblack::Locator::entitiesRegistry::emplace<openblack::ecs::Registry>();
	auto& registry = openblack::Locator::entitiesRegistry::value();
	const auto pile = registry.Create();
	const auto other = registry.Create();
	EXPECT_FALSE(object_flags::IsInLockedSelect(pile));
	object_flags::SetInLockedSelect(pile, true);
	EXPECT_TRUE(object_flags::IsInLockedSelect(pile));
	EXPECT_FALSE(object_flags::IsInLockedSelect(other));
	// set again it stays set; clearing an object never set does nothing
	object_flags::SetInLockedSelect(pile, true);
	object_flags::SetInLockedSelect(other, false);
	EXPECT_TRUE(object_flags::IsInLockedSelect(pile));
	EXPECT_FALSE(object_flags::IsInLockedSelect(other));
	object_flags::SetInLockedSelect(pile, false);
	EXPECT_FALSE(object_flags::IsInLockedSelect(pile));
	// an object gone takes its flag with it, and one gone can't be set; no object is never in a select
	object_flags::SetInLockedSelect(pile, true);
	registry.Destroy(pile);
	EXPECT_FALSE(object_flags::IsInLockedSelect(pile));
	object_flags::SetInLockedSelect(pile, true);
	EXPECT_FALSE(object_flags::IsInLockedSelect(pile));
	EXPECT_FALSE(object_flags::IsInLockedSelect(entt::null));
}
