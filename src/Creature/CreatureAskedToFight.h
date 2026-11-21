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

#include "Creature/CreatureFight.h"

/// What the mind of a creature asked to fight does before the creature that asked it gives it its first order. The
/// creature that asks leads the face-off by its own turns: on the first it asks, on the second it tells the mind of the
/// other creature to stand still, and on the third it gives it its first order. Until that order, the other creature
/// carries on with what it did.
namespace openblack::creature_asked_to_fight
{
/// How far a creature takes part in the fight it is in. The creature that asked takes part from the start
enum class Part : uint8_t
{
	/// It was asked, and agreed. Its mind goes on with what it did, and a new plan it takes calls the fight off
	Asked,
	/// Its mind stands still: its desires do not change and it chooses nothing, but its body goes on with what it did
	StandsStill,
	/// It stopped what it did, and the fight moves its body
	Obeys,
};

/// What the mind reads of a creature's fight
struct Fight
{
	/// It asked for the fight
	bool askedForIt {false};
	/// The creature that asked is still in its face-off, and the turns of the face-off it has led so far
	bool askerFacesOff {false};
	uint32_t askerFaceOffTurns {0};
	/// It won by a knock-out and is made to do something to the loser
	bool won {false};
};

/// How far the creature takes part in its fight
[[nodiscard]] constexpr Part PartOf(const Fight& fight)
{
	if (fight.askedForIt || !fight.askerFacesOff || fight.askerFaceOffTurns >= creature_fight::k_FaceOffOrderTurn)
	{
		return Part::Obeys;
	}
	return fight.askerFaceOffTurns < creature_fight::k_FaceOffStillTurn ? Part::Asked : Part::StandsStill;
}

/// Whether the mind leaves the body to the fight: from the creature's first order, and always for the creature that
/// asked
[[nodiscard]] constexpr bool FightHasBody(const Fight& fight)
{
	return PartOf(fight) == Part::Obeys;
}

/// Whether the mind of the creature asked stands still: its desires do not change and it chooses nothing, from the turn
/// the asker tells it to stand still until the fight is over, as the asker's order to stand still lasts until it lets
/// the mind go again. A winner made to do something to the loser thinks again
[[nodiscard]] constexpr bool MindStandsStill(const Fight& fight)
{
	return !fight.askedForIt && !fight.won && PartOf(fight) != Part::Asked;
}

/// Whether a new plan the creature takes calls its fight off: only before its mind is told to stand still. The asker
/// stops then, as a creature whose partner stops what it does for a joint action
[[nodiscard]] constexpr bool NewPlanCallsOff(const Fight& fight)
{
	return PartOf(fight) == Part::Asked;
}
} // namespace openblack::creature_asked_to_fight
