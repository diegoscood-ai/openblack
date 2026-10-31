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

#include <array>
#include <optional>
#include <span>

#include <entt/entity/entity.hpp>

#include "Creature/CreatureHandRules.h"

/// The hand holding a creature, frame by frame: the locked select that takes hold of it and lets it go, the time the
/// hand holds it (a click when short), and when the render hand enters and leaves its CREATURE state. Pure steps: the
/// hand reads the facts, and does what the step says. Wiki: docs/wiki/hand/states.md#the-hand-on-a-creature.
namespace openblack::ecs::hand_creature
{

/// How far the hand's locked select on a creature is: waiting for the turn to lock it, held, let go and waiting to end
/// it, then waiting for the end to be applied
enum class LockPhase : uint8_t
{
	Waiting,
	Held,
	WaitingForLockOff,
	Completion,
};

struct LockState
{
	LockPhase phase {LockPhase::Waiting};
	/// The action button went up since the press on the creature, kept until the held phase reads it, as the interface
	/// keeps its button bits
	bool released {false};
};

/// What the hand knows about its locked creature this frame
struct LockFacts
{
	/// The creature is still there
	bool available {false};
	/// The start packet locked it for the hand
	bool locked {false};
	/// The hand's locked select is still on this creature (a lock the turn refused has been cleared from it)
	bool stillSelected {false};
	/// The action button went up this frame
	bool actionReleased {false};
};

struct LockStep
{
	LockState next;
	/// Send the end of the locked select
	bool sendEnd {false};
	/// The locked select is over: the hand lets the creature go and forgets it (after the end, when both are set)
	bool letGo {false};
};

/// One frame of the creature's locked select: held once locked, let go at the release, ended one frame later, and over
/// once the end is applied. A creature no longer there ends it at once
[[nodiscard]] LockStep StepLock(const LockState& state, const LockFacts& facts);

/// Whether a locked select's start packet takes its object: there is one, it is interactable, and it is not in a locked
/// select already. Otherwise the start only ends the action. Wiki: docs/wiki/hand/states.md#the-start
[[nodiscard]] constexpr bool StartTakesObject(bool present, bool interactable, bool inLockedSelect)
{
	return present && interactable && !inLockedSelect;
}

/// A locked select's packet, as the selected object's own locked-select flag sees it
enum class SelectFlagEvent : uint8_t
{
	/// A start packet took the object, whatever the object's own start then gave (a scoop that took nothing included)
	StartTaken,
	/// A start packet did not take it (StartTakesObject)
	StartRefused,
	/// The end packet is applied to the object
	End,
};

/// Whether a locked select's end packet ends the hand's action: with no object (gone), or one that is no longer
/// interactable. An object that is there is then taken out of the select, interactable or not.
/// Wiki: docs/wiki/hand/states.md#the-action-states-3-to-6
[[nodiscard]] constexpr bool EndPacketEndsAction(bool present, bool interactable)
{
	return !present || !interactable;
}

/// The object's locked-select flag, which the scripts read (IS_LOCKED_INTERACTION): set by a start that took it and
/// cleared only by its end packet. The turn's pass that ends a select early (a scoop that took nothing, a source gone
/// or out of the influence) only sends that end, so the object stays in the select until the end is applied at the
/// next turn. Wiki: docs/wiki/hand/states.md#what-the-scripts-read-of-it
[[nodiscard]] constexpr bool StepLockedSelectFlag(bool flag, SelectFlagEvent event)
{
	switch (event)
	{
	case SelectFlagEvent::StartTaken:
		return true;
	case SelectFlagEvent::End:
		return false;
	case SelectFlagEvent::StartRefused:
		break;
	}
	return flag;
}

/// What the turn's pass knows about the interface's locked object (the object of any locked select, from its start until
/// its end is applied)
struct LockedObjectFacts
{
	/// The object is still there
	bool available {false};
	/// The object is a creature, which the hand keeps outside the influence too
	bool creature {false};
	/// The player's influence at the hand's synced point (read only for an object still there that is not a creature)
	float influence {0.0f};
	/// The object is the field, fish farm or pile the hand takes from, whose own pass ends the action
	bool pickSource {false};
	/// Its interaction this turn goes on: no for a field, fish farm or pile the hand took nothing from (no handful in the
	/// hand), yes for anything else
	bool interacts {true};
};

struct LockedObjectStep
{
	/// The interface lets go of its locked object
	bool letGo {false};
	/// The hand's action ends: the select's end, then the action's
	bool endAction {false};
};

/// One turn of the interface's locked object: let go of once it is no longer there, or, when it is not a creature, once
/// the hand has no influence (0 or less), or once its interaction is over (a source that gave nothing); the action ends
/// with it but for the source the hand takes from, whose own pass ends its own. Wiki: docs/wiki/hand/states.md#each-turn
[[nodiscard]] constexpr LockedObjectStep StepLockedObject(const LockedObjectFacts& facts)
{
	const bool letGo = !facts.available || (!facts.creature && !(facts.influence > 0.0f)) || !facts.interacts;
	return {.letGo = letGo, .endAction = letGo && !facts.pickSource};
}

/// A locked select's packet, as the interface's creature-lock flag sees it
enum class LockFlagEvent : uint8_t
{
	/// A creature's start is applied
	CreatureStart,
	/// A creature's end is applied
	CreatureEnd,
	/// Any other start or end, the end of an object that is gone among them (it reaches its handler as no object)
	Other,
};

/// The interface's flag that a creature is locked for its hand, which the gestures and the hand's readiness for an
/// object read: set when a creature's start is applied, cleared only when a creature's end is applied, so a creature
/// gone while locked leaves it set. Wiki: docs/wiki/hand/states.md#each-turn
[[nodiscard]] constexpr bool StepCreatureLockFlag(bool flag, LockFlagEvent event)
{
	switch (event)
	{
	case LockFlagEvent::CreatureStart:
		return true;
	case LockFlagEvent::CreatureEnd:
		return false;
	case LockFlagEvent::Other:
		break;
	}
	return flag;
}

/// Whether the hand is ready for an object (a seed, a worship icon's seed, a caught fireball): there is space in the
/// hands, then no creature is locked for the hand (the interface's flag). Nothing else is asked: a tree being pulled
/// is not in the hand yet, and a source being scooped has already put its handful there.
/// Wiki: docs/wiki/hand/states.md#each-turn
[[nodiscard]] constexpr bool IsHandReadyForObject(bool spaceInHands, bool creatureLockedForHand)
{
	return spaceInHands && !creatureLockedForHand;
}

/// The render hand's state while it holds a creature
constexpr int32_t k_CreatureHandState = 7;

/// The time the hand holds a creature, in camera ms, from the first frame of the Creature Interaction hand state
struct Interaction
{
	bool started {false};
	uint32_t cameraMs {0};
};

/// What the hand knows about the creature in it this frame
struct FrameFacts
{
	/// The interface's hand state is Creature Interaction
	bool creatureInteraction {false};
	/// The render hand has a creature
	bool hasCreature {false};
	/// This frame's camera ms
	uint32_t cameraMs {0};
	/// The state the render hand is asked for this frame
	int32_t requiredState {0};
	/// The state the render hand is in
	int32_t renderState {0};
	/// The creature is locked for the hand
	bool creatureLocked {false};
	/// The locked creature is still finishing the body action it was doing when the hand took hold of it
	bool creatureFinishing {false};
};

/// What the hand sends, in this order
enum class Send : uint8_t
{
	/// The click packet: the player's leash key
	Click,
	/// Leaving the CREATURE state: the creature is let go, and how it was treated is sent to its mind
	Feedback,
};

struct FrameStep
{
	Interaction interaction;
	/// The render hand's state from this frame
	int32_t renderState {0};
	std::array<Send, 2> sends {};
	uint8_t sendCount {0};

	[[nodiscard]] std::span<const Send> Sends() const { return std::span(sends).first(sendCount); }
};

/// Whether a creature locked for the hand is still finishing the body action it was doing when taken hold, after its
/// turn: it starts finishing as it is locked, and is done the first turn its body plays no action, from then on until it
/// is let go
[[nodiscard]] constexpr bool StillFinishing(bool finishing, bool locked, bool bodyActing)
{
	return locked && finishing && bodyActing;
}

/// Whether the creature's body is taken off the game this turn: the turn its locked creature stops finishing, as the
/// hand's creature state begins. From then until the creature is let go the game reads the pose it had then, while the
/// body drawn goes on playing
[[nodiscard]] constexpr bool TakesBodyOffTheGame(bool wasFinishing, bool finishing, bool locked)
{
	return locked && wasFinishing && !finishing;
}

/// One frame of the render hand with a creature. The camera ms count on every frame and are read only between the
/// interaction's first frame and its end; let go of the player's own creature within a click's time, the click is sent.
/// The CREATURE state begins only once the creature is locked and has finished its body action, and leaving it sends the
/// feedback, after the click.
/// `ownCreature` says whether the render hand's creature is the player's, and is asked only when the interaction ends
template <typename OwnCreature>
[[nodiscard]] FrameStep StepFrame(const Interaction& interaction, const FrameFacts& facts, OwnCreature&& ownCreature)
{
	FrameStep step {.interaction = interaction, .renderState = facts.renderState};
	step.interaction.cameraMs += facts.cameraMs;
	if (facts.creatureInteraction)
	{
		if (facts.hasCreature && !step.interaction.started)
		{
			step.interaction.started = true;
			step.interaction.cameraMs = 0;
		}
	}
	else if (step.interaction.started)
	{
		step.interaction.started = false;
		if (creature_hand::IsClick(step.interaction.cameraMs) && facts.hasCreature && ownCreature())
		{
			step.sends[step.sendCount++] = Send::Click;
		}
	}
	auto state = facts.requiredState;
	if (state == k_CreatureHandState && facts.renderState != k_CreatureHandState &&
	    (!facts.creatureLocked || facts.creatureFinishing))
	{
		// until the creature is locked for the hand and has finished what its body was doing, the hand stays as it is
		state = facts.renderState;
	}
	if (state != facts.renderState && facts.renderState == k_CreatureHandState)
	{
		step.sends[step.sendCount++] = Send::Feedback;
	}
	step.renderState = state;
	return step;
}

} // namespace openblack::ecs::hand_creature
