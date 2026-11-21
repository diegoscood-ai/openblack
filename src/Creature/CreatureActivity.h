/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <algorithm>
#include <iterator>
#include <optional>
#include <span>
#include <vector>

#include "Creature/CreatureDesires.h"

/// The rules for the whole thing that a desire is about. A desire about something (to impress, compassion, anger, the
/// desires about other creatures) is about a whole thing the creature believes in: a town, a forest, a temple, a flock or
/// a creature. Everything here is pure.
namespace openblack::creature_activity
{

/// The kinds of whole thing that a desire can be about
enum class Whole : uint8_t
{
	Town,
	Forest,
	Citadel,
	Creature,
	Flock,
};

/// The kind of whole thing that a belief kind is, none for a kind that is not a whole thing
[[nodiscard]] std::optional<Whole> WholeOf(uint32_t beliefKind);

/// What the creature knows about a whole thing when it weighs a desire about it
struct WholeFacts
{
	Whole kind {Whole::Town};
	/// The whole thing's player is the creature's player; a thing of no player and a creature of no player count as the
	/// same, as the game compares them
	bool samePlayer {false};
	/// The whole thing has a player
	bool hasPlayer {false};
	/// For a town: the town's belief in the creature's player is more than half of its largest belief in another
	/// player; always, for a creature of no player
	bool believesInPlayer {false};
	/// The creature's player looks at the creature
	bool lookedAt {false};
	/// For a creature: the creature can attack it
	bool attackable {false};
};

/// Whether a desire can be about a whole thing.
/// - The desire to impress is about a town, but not a town of the creature's own player that believes in that player
///   more than half as much as in any other.
/// - Compassion is about a town or a creature.
/// - Anger is about a forest, a flock, a temple of another player, a creature that it can attack, or a town; a town of
///   its own player counts only while the player looks at the creature. While it must not attack its own player's
///   things, nothing of that player counts.
/// - The desires about other creatures are about a creature.
/// - No other desire is about a whole thing.
[[nodiscard]] bool CanBeAbout(creature_desires::Desire desire, const WholeFacts& whole, bool mayAttackOwnPlayer);

/// Whether compassion for a whole thing gives the creature actions to do: only a town's does
[[nodiscard]] bool CompassionHasActions(Whole kind);

/// The first of the values that is the most and more than nothing, none when no value is more than nothing
[[nodiscard]] std::optional<size_t> MostUseful(std::span<const float> usefulness);

/// A whole thing a creature believes in, as it weighs it as what a plan for a desire is about
struct WholeCandidate
{
	WholeFacts facts;
	/// The whole thing is the creature itself
	bool itself {false};
	/// Its worth as the goal (how useful it is as one, times how the distance weighs against it)
	float worth {0.0f};
};
/// What a plan for a desire that looks for a goal is about: of the whole things the creature believes in, in the order
/// given (the newest first), not itself and those the desire can be about (CanBeAbout), the first of the most worth,
/// and only one worth more than nothing
[[nodiscard]] std::optional<size_t> ChooseWhole(creature_desires::Desire desire, std::span<const WholeCandidate> wholes,
                                                bool mayAttackOwnPlayer);

/// What a desire acts on when its player sends the creature to act on a thing, in the order it tries them until one
/// gives a plan: the thing alone when it suits the desire (suits: it may act on it and it is of some use to it); else,
/// of the things the creature took to belong to the thing's whole thing, in the order given (the newest belief first),
/// each that suits. Nothing suits: the desire has nothing to act on
template <typename Thing, typename Suits>
[[nodiscard]] std::vector<Thing> SentToActOn(Thing thing, std::span<const Thing> parts, Suits suits)
{
	if (suits(thing))
	{
		return {thing};
	}
	std::vector<Thing> order;
	std::ranges::copy_if(parts, std::back_inserter(order), suits);
	return order;
}

} // namespace openblack::creature_activity
