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

namespace openblack::ecs::components
{

/// A puzzle totem: a totem building of no town that a script makes (CREATE of a totem: the bell towers and the undead
/// village's totems on the fourth land) or a totem puzzle game lays out. It stands out of the ground by its height,
/// a whole number of steps from 0 (sunk) to its highest, which scripts read and set (HEIGHT, MAX_HEIGHT), the
/// creature pulls up and down and the hand drags (puzzle_totem). It is not the village totem (VillageTotem), the
/// statue on a town centre.
struct PuzzleTotem
{
	/// Its highest step; puzzle_totem::k_TapOnly for a totem that is only tapped, never raised
	int32_t maxHeight {100};
	/// How many steps it stands raised
	int32_t height {0};
	/// The height a totem puzzle game last moved it from, and how many more of its reads blend from it
	int32_t previousHeight {0};
	int32_t blendCountdown {0};
	/// Its altitude above the land with all its height: where it was made
	float restAltitude {0.0f};
	/// Its altitude above the land now: the rest altitude less the part of its travel its height leaves sunk
	float altitude {0.0f};
	/// Whether the hand holds it (from the locked select's start packet to its end packet)
	bool held {false};
	/// The hand's drag: how far below its rest altitude the hand has pulled it (0 down to minus its travel), the
	/// altitude the hand holds it at, which it is drawn at while held, and the altitude last sent
	float dragOffset {0.0f};
	float handAltitude {0.0f};
	float sentAltitude {0.0f};
};

} // namespace openblack::ecs::components
