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

/// The hand's action press: which of its branches a press takes. The hand tries them in a fixed order and the first that
/// holds wins, so the order is the whole precedence. Wiki: docs/wiki/hand/states.md#the-action-press.
namespace openblack::ecs::hand_press
{

/// The branch an action press takes, in the order the hand tries them
enum class Branch : uint8_t
{
	None,
	/// A bubble on the screen: the hand grips it
	ScreenObject,
	/// The tied leash's rope under the empty hand: the hand takes hold of it and pulls
	LeashPull,
	/// A field under the hand: its scooping
	Field,
	/// Any other object under the hand: a pile's scooping, a tap, a tug or a pick-up
	Hovered,
	/// A village totem under the empty hand: the hand takes hold of it
	Totem,
	/// An object that can only be tapped: an abode, a temple's entrance or a "Did you know?" sign
	TapOnly,
	/// The player's own creature: the hand takes hold of it
	Creature,
	/// The fish of a fish farm at the hand's point, with no creature under the cursor: their scooping
	FishFarm,
	/// A spell seed in the hand
	Seed,
	/// Anything else in the hand
	Held,
};

/// What the hand knows at an action press. None of them changes anything as it is read
struct Facts
{
	/// A screen object (a bubble) is under the empty hand, which is not gripping the land
	bool screenPress {false};
	/// The tied leash's rope took the pick under the empty hand
	bool ropePress {false};
	bool held {false};
	bool hovered {false};
	bool hoveredIsField {false};
	bool gripping {false};
	/// The object under the cursor is a village totem's plinth or icon, available
	bool cursorIsTotem {false};
	/// The object under the cursor is available and can only be tapped
	bool tapOnlyCursorObject {false};
	/// The object under the cursor is an available creature, whoever's
	bool cursorIsCreature {false};
	/// The object under the cursor is a creature the hand takes hold of (creature_hand::TakesPress)
	bool creatureTakesPress {false};
	/// The hand's point is in the influence, over the shown fish of a fish farm. Not looked for over a creature
	bool fishFarmInInfluence {false};
	bool holdingSeed {false};
	/// The press that picked up what the hand holds is still down
	bool pickPressHeld {false};
	/// The hand holds a creature or is letting one go: no other press is taken until it is done
	bool creatureLockBusy {false};
	/// A lesson's interface level keeps the empty hand from the leash's rope and from every locked select (a creature,
	/// a field, a pile, a totem, the fish): such a press goes straight on to the pick-up path (LessonSkipsSelects)
	bool lessonSkipsSelects {false};
};

/// The interface's flags (Input/InterfaceActive.h) a lesson's interface level sets for the empty hand's press. Every
/// lesson level but the hand interaction one sets both; the normal level and the two that let the player rotate and
/// interact clear them
inline constexpr uint8_t k_LessonOnlyHighlights = 2u;
inline constexpr uint8_t k_LessonSkipsSelects = 4u;

/// Whether the lesson keeps the empty hand from the leash's rope and from every locked select. A bubble on the screen
/// is still gripped, and what the hand holds is still pressed as before
[[nodiscard]] constexpr bool LessonSkipsSelects(uint8_t interfaceFlags)
{
	return (interfaceFlags & k_LessonSkipsSelects) != 0;
}

/// Whether the lesson refuses a press of the empty hand on its pick-up path (a pick-up, a tap, a click on the land):
/// it lets the hand act only on a script's highlight, and the thing at the press is none. A refused press does nothing
[[nodiscard]] constexpr bool LessonRefusesPress(uint8_t interfaceFlags, bool onHighlight)
{
	return (interfaceFlags & k_LessonOnlyHighlights) != 0 && !onHighlight;
}

/// The first branch whose conditions hold, in the hand's order; None when none does
[[nodiscard]] Branch Choose(const Facts& facts);

/// What the totem, tap-only, creature and fish farm branches share: an empty hand over nothing, not gripping the land.
/// Their own facts need reading only when this holds, as the hand did before it chose this way. A rope the lesson keeps
/// the hand from is as nothing
[[nodiscard]] constexpr bool EmptyHandOverNothing(const Facts& facts)
{
	return !facts.screenPress && !(facts.ropePress && !facts.lessonSkipsSelects) && !facts.held && !facts.hovered &&
	       !facts.gripping && !facts.creatureLockBusy;
}

/// Whether a press on an object may start its locked select: the object allows one, can be picked up and is not in a
/// locked select already (the start packet of an earlier press took it and its end is not applied yet). Otherwise the
/// press goes on as for an object the hand can't select. Wiki: docs/wiki/hand/states.md#the-action-press
[[nodiscard]] constexpr bool PressMayLockSelect(bool validForLockedSelect, bool cannotBePickedUp, bool inLockedSelect)
{
	return validForLockedSelect && !cannotBePickedUp && !inLockedSelect;
}

/// What a press on a thing that has a locked select (a field, a fish farm) does when the select can't start (out of the
/// influence, refused by the thing, or the thing still in the select of an earlier press)
enum class RefusedSelect : uint8_t
{
	/// The grab start, which taps the thing out of the influence and otherwise takes hold of it
	Grab,
	/// The hand's click on the land at the action's point
	LandClick,
};
/// The press goes on to the grab start when the hand may take the thing and no script made it unpickable, or while the
/// untied leash is on; otherwise the land is clicked. Wiki: docs/wiki/hand/states.md#the-action-press
[[nodiscard]] constexpr RefusedSelect RefusedSelectPress(bool placeable, bool cannotBePickedUp, bool leashOnUntied)
{
	return (placeable && !cannotBePickedUp) || leashOnUntied ? RefusedSelect::Grab : RefusedSelect::LandClick;
}

/// What a press that takes no branch does with an empty hand over nothing it can hold, tap or select
enum class FallThrough : uint8_t
{
	/// Something else holds the hand: nothing
	None,
	/// The hand's click on the land at the action's point
	LandClick,
	/// The thing found at the action's point is tapped, which the untied leash takes to send the creature to it
	TapThing,
};
/// With a thing found at the press's point that no branch takes: tapped while the player's leash is on and not tied,
/// else, as with no thing, the land is clicked
[[nodiscard]] constexpr FallThrough PressFallsThrough(bool emptyHandOverNothing, bool thingAtPoint, bool leashOnUntied)
{
	if (!emptyHandOverNothing)
	{
		return FallThrough::None;
	}
	return thingAtPoint && leashOnUntied ? FallThrough::TapThing : FallThrough::LandClick;
}

/// A puzzle totem under the empty hand that the press does not take hold of (one the script made unpickupable, one
/// that is not working, held or out of the influence) goes on as a thing no branch takes: the land is clicked at the
/// hand's point, unless the totem can be tapped or the untied leash is on, when it is tapped (the leash's tap sends the
/// creature to it). Taken hold of: nothing more. Wiki: docs/wiki/magic/puzzle-totem.md#the-hand-and-the-tap
[[nodiscard]] constexpr FallThrough TotemPressFallsThrough(bool tookHold, bool validToTap, bool leashOnUntied)
{
	if (tookHold)
	{
		return FallThrough::None;
	}
	return validToTap || leashOnUntied ? FallThrough::TapThing : FallThrough::LandClick;
}

/// Whether the idle hand takes the action button's press (and its double click) this frame: not while the game is
/// paused, when it takes only the land grip, so the camera still moves but nothing is picked up, tapped, dropped, given
/// or cast
[[nodiscard]] constexpr bool IdleTakesActionPress(bool paused)
{
	return !paused;
}

} // namespace openblack::ecs::hand_press
