/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

/// The interface's check of its action against the objects the action needs, as the map's clear runs it: an action
/// whose objects are gone ends, and an interface left active under a script's wide screen is made inactive. Wiki:
/// docs/wiki/hand/states.md#clearing-on-a-new-land.
namespace openblack::ecs::hand_validate
{

/// The number of the interface's action states
inline constexpr int32_t k_StateCount = 24;
/// The action state of an inactive interface: its action is not ended
inline constexpr int32_t k_InactiveState = 14;

/// What an action state needs to go on
struct StateNeeds
{
	/// The action's object (the object the action's collide holds)
	bool actionObject {false};
	/// An object in the hand
	bool heldObject {false};
};

/// The needs of each state, from the action states' table: the wait for the place in the hand (7) and the grab (13)
/// need the action's object; the seed's applies (8 to 11), the release (12), the give (15) and state 20 need what the
/// hand holds; the others need neither
[[nodiscard]] constexpr StateNeeds NeedsOf(int32_t state) noexcept
{
	switch (state)
	{
	case 7:
	case 13:
		return {.actionObject = true};
	case 8:
	case 9:
	case 10:
	case 11:
	case 12:
	case 15:
	case 20:
		return {.heldObject = true};
	default:
		return {};
	}
}

/// What the check reads
struct Facts
{
	/// The interface is active
	bool active {true};
	/// A script holds the wide screen (the cinema bars)
	bool scriptHoldsWideScreen {false};
	/// A hand demo plays
	bool demoPlaying {false};
	/// The interface's action state
	int32_t state {0};
	/// The action's collide holds an object
	bool actionObject {false};
	/// That object can still be acted on (not deleted)
	bool actionObjectInteractable {false};
	/// That object is in a hand
	bool actionObjectInHand {false};
	/// The hand holds an object that can still be acted on
	bool holdsInteractable {false};
};

enum class Outcome : uint8_t
{
	/// The action goes on
	Keep,
	/// The action ends (the state's end, the action's objects cleared, the action state reset)
	EndAction,
	/// The interface is made inactive, which ends the action too
	SetInactive,
};

/// The check, in its order: an active interface under a script's wide screen with no hand demo playing is made
/// inactive; an inactive interface not yet in its inactive state ends its action; then the action ends when its object
/// is gone or in a hand, or when it has none and its state needs one, or when the hand holds nothing that can still be
/// acted on and the state needs it. Each end resets the action state to one that needs nothing, so at most one end is
/// made
[[nodiscard]] constexpr Outcome Validate(const Facts& facts) noexcept
{
	if (facts.active && facts.scriptHoldsWideScreen && !facts.demoPlaying)
	{
		return Outcome::SetInactive;
	}
	if (!facts.active && facts.state != k_InactiveState)
	{
		return Outcome::EndAction;
	}
	const auto needs = NeedsOf(facts.state);
	if (facts.actionObject ? !facts.actionObjectInteractable || facts.actionObjectInHand : needs.actionObject)
	{
		return Outcome::EndAction;
	}
	if (!facts.holdsInteractable && needs.heldObject)
	{
		return Outcome::EndAction;
	}
	return Outcome::Keep;
}

} // namespace openblack::ecs::hand_validate
