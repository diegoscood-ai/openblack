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

#include "Enums.h"

/// What a tree put down upright on land does as it is planted again, and in which order. Pure, so that the order can be
/// tested on its own; the hand's replanting runs the steps.
namespace openblack::tree_replant
{

enum class Step : uint8_t
{
	/// A puff of smoke on the ground under it, before anything else
	Smoke,
	/// The plant-object feel on the mouse of the player who dropped it (force feedback)
	Immersion,
	/// The search around it for a town or a forest to join
	FindForest,
	/// It leaves its old forest, takes the "in a town" answer, and joins the forest found, or outside a town a new one
	JoinForest,
	/// The forest-created spot visual, outside a town
	SpotVisual,
	/// The dropper's creature may learn to copy planting a tree
	CreatureMimic,
	/// The dropper's alignment: planting a tree is good
	Alignment,
	/// It goes back into its map cell
	BackInMap,
};

/// The steps in the order they run
inline constexpr std::array k_Order {
    Step::Smoke,      Step::Immersion,     Step::FindForest, Step::JoinForest,
    Step::SpotVisual, Step::CreatureMimic, Step::Alignment,  Step::BackInMap,
};

/// The force feedback effect of the immersion step, with the flag the original passes it
inline constexpr ImmersionEffectType k_Immersion = ImmersionEffectType::PlantObject;
inline constexpr uint32_t k_ImmersionFlags = 0x80000000u;

/// What decides whether a step runs, as it stands when the step is reached
struct Facts
{
	/// Someone's hand dropped it (the immersion goes to that player's interface)
	bool dropped {false};
	/// The search found it inside a town (known from the search on)
	bool inTown {false};
	/// Its body has a player (the creature's mimicry and the alignment are that player's)
	bool player {false};
};

/// Whether a step runs
[[nodiscard]] constexpr bool Runs(Step step, const Facts& facts)
{
	switch (step)
	{
	case Step::Immersion:
		return facts.dropped;
	case Step::SpotVisual:
		return !facts.inTown;
	case Step::CreatureMimic:
	case Step::Alignment:
		return facts.player;
	default:
		return true;
	}
}

} // namespace openblack::tree_replant
