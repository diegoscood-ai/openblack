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

#include "Enums.h"

// How a fully open vortex that leads away from the land takes a creature it reaches. A creature is only taken when a
// script lets it through and it is on the leash, or whenever a script controls it, and then only once it comes close to
// the middle, where it sparkles away for good. A thing flying in the physics that strikes it while it is fully open is
// taken as it flies. Pure rules, tested without the game.

namespace openblack::vortex
{

/// Whether a creature may be taken by a vortex that leads away: let through by a script while on the leash, or
/// controlled by a script
[[nodiscard]] bool CreatureMayBeTaken(bool letThrough, bool leashed, bool controlledByScript);

/// A creature that may be taken sparkles away once nearer the middle than this share of the vortex's radius across the
/// land
inline constexpr float k_CreatureTakenShare = 0.7f;
/// It sparkles away over this many seconds, right out of sight, and is gone for good
inline constexpr float k_CreatureSparkleSeconds = 3.0f;
/// The fizz it sparkles away to: all of it
inline constexpr float k_CreatureSparkleTarget = 1.0f;

/// A way out of a land pulls things in from the land only while it is fully open, not while it fades in or out, as the
/// original (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
[[nodiscard]] constexpr bool PullsFromTheLand(VortexStateType state)
{
	return state == VortexStateType::Active;
}

/// It walks at most this many cells round its own in a turn, as the original
/// (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
inline constexpr int k_PullSpiralCells = 9999;

/// How far it reaches on a turn: its whole radius on every third turn and half of it on the others, as the original
/// (docs/wiki/world/vortex.md#taking-objects-in-the-in-vortex)
[[nodiscard]] constexpr float PullReach(float radius, uint32_t turn)
{
	return turn % 3 == 0 ? radius : radius * 0.5f;
}

/// Whether a vortex takes a thing that struck it as it flew: only a way out of a land, only while fully open, and only a
/// thing still flying in the physics that it would draw in from the land
[[nodiscard]] bool TakesWhatStrikesIt(VortexType type, VortexStateType state, bool hitterFlying, bool hitterMayBeTaken);

} // namespace openblack::vortex
