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

#include <glm/vec3.hpp>

/// Tying the leash with the hand. With the leash in the hand, a double click of the Action button on something ties the
/// leash to it, and a double click on what it is tied to, or on the creature, unties it back to the hand. A single tap
/// with the leash in the hand sends the creature to act on what was tapped, a thing or a point on the land.
///
/// The leash can be tied to anything in the world but a temple's leash posts, a spell icon, a script highlight and its
/// own creature. A double click on a one-off spell seed taps it instead.
namespace openblack::creature_leash
{

/// What the hand points at, as the tie sees it
enum class TieTargetKind : uint8_t
{
	/// Anything else: a tree, a building, a villager, an animal, a village centre, a totem, a rock, another creature
	Object,
	/// One of a temple's three leash posts
	LeashPost,
	/// A spell icon: a worship site's, a village centre's or a town's
	SpellIcon,
	/// A script's scroll or sign on the map
	ScriptHighlight,
	/// A one-off spell seed lying on the land
	OneOffSpellSeed,
};

/// Whether the hand may act on it at all with the leash: everything but a script highlight
[[nodiscard]] bool ValidAsTarget(TieTargetKind kind);
/// Whether a leash may be tied to it: everything but a leash post or a spell icon
[[nodiscard]] bool ValidAsLeashTarget(TieTargetKind kind);

/// What decides a double click of the Action button
struct HandTieCheck
{
	/// The hand points at something (what is under it, or else the nearest thing near where it acts)
	bool hasTarget {false};
	TieTargetKind kind {TieTargetKind::Object};
	/// The player has a creature, and it is their own
	bool hasCreature {false};
	/// The target is the player's creature
	bool targetIsCreature {false};
	/// The player's leash is tied to something, and the target is that thing
	bool tied {false};
	bool targetIsTiedObject {false};
	/// The creature's leash is on, held in this player's hand
	bool leashInThisHand {false};
};

enum class HandTie : uint8_t
{
	/// Nothing happens
	Nothing,
	/// The target is tapped, as a single click would
	Tap,
	/// The leash is tied to the target
	Tie,
	/// The leash is untied, back to the hand
	Untie,
};

/// The double click, in the original's order: with no target, or no creature of the player's own, nothing. Tied
/// already: a double click on what it is tied to, or on the creature, unties it, and anything else does nothing. Not
/// tied: the target must not be the creature, the hand must be able to act on it and the leash must be in this
/// player's hand; then a one-off spell seed is tapped, and anything a leash may be tied to is tied to.
[[nodiscard]] HandTie DecideHandTie(const HandTieCheck& check);

/// What decides a single tap with the leash in the hand
struct LeashTapCheck
{
	/// This player's leash is on and not tied to anything
	bool leashOnUntied {false};
	/// The player has a creature
	bool hasCreature {false};
	/// The creature's leash is held in this player's hand
	bool leashInThisHand {false};
	/// The creature is already carrying out the step the leash's act sends it on (step 93 of its current plan). Open
	/// point: openblack keeps no such steps on a creature yet, so it is never set
	bool alreadyActingForLeash {false};
};

enum class LeashTap : uint8_t
{
	/// Not the leash's: the hand taps as it always does
	NotLeash,
	/// The creature is sent to act on the thing tapped
	ActOnObject,
	/// The creature is sent to act on the point of the land tapped
	ActOnPoint,
	/// A tap on the land at the origin: taken, and nothing happens, neither the leash's act nor the hand's own tap
	Nothing,
};

/// A tap on a thing: with the leash on and untied, a thing the hand may act on and a leash may be tied to, the leash in
/// this player's hand and the creature not already doing what the leash sent it to do, the creature acts on it.
/// Anything else is the hand's own tap.
[[nodiscard]] LeashTap DecideLeashTapOnObject(const LeashTapCheck& check, TieTargetKind kind);
/// Whether a point tapped on the land is the origin, which the tap ignores: its first two coordinates are compared bit
/// for bit with zero, so -0 there is not the origin, and the third as a number, so -0 there is, and so is a NaN (the
/// comparison is unordered, which counts as equal)
[[nodiscard]] bool IsTapOrigin(const glm::vec3& point);
/// A tap on the land. At the origin nothing happens at all, whatever the leash. Elsewhere, with the leash on and
/// untied, a creature and the creature not already doing what the leash sent it to do, the creature acts on the point.
/// Unlike the tap on a thing, the leash need not be in this player's hand. Anything else is the hand's own tap.
[[nodiscard]] LeashTap DecideLeashTapOnLand(const LeashTapCheck& check, bool pointIsOrigin);

} // namespace openblack::creature_leash
