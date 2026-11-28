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

#include "Creature/CreatureDesires.h"

/// The rules of a creature's mind turn that come before it decides what to do
namespace openblack::creature_turn_order
{
/// Before it grows up to its first stage, the desires of its body are set to 0 each turn before they grow. Each turn
/// adds only one turn of growth to them, so the creature does not act on them.
constexpr uint32_t k_FirstStageWithBody = 1;
void HoldBodyDesires(creature_desires::Desires& desires, const std::array<bool, creature_desires::k_DesireCount>& body,
                     uint32_t phase);

/// After the hand held it, the first turn that finds a plan with steps to carry out starts that plan again from its first
/// step. The mark that the hand held it stays until then, or until what it does stops.
[[nodiscard]] bool RestartsAfterHold(bool heldSinceStop, bool planHasSteps);

/// A player's creature that has set off on nothing since it last stopped what it did (no plan of its own, a plan to
/// follow the player not counting, and no walk started) counts the turns. Past 15 seconds of them it stops copying the
/// player, its player's order is over, and it does not want to follow the player for 60 seconds.
constexpr float k_StuckSeconds = 15.0f;
constexpr float k_StuckNoFollowSeconds = 60.0f;
/// Whether a plan under way counts as setting off: one with steps to carry out, for any desire but to follow the player
[[nodiscard]] bool SetsOff(bool planHasSteps, creature_desires::Desire desire);
/// One turn of the count: it grows on each turn it has not set off since it stopped, and goes back to 0 only when the
/// time is up, which this returns true for. The limit is a whole number of turns, the fraction dropped.
[[nodiscard]] bool StepStuck(uint32_t& turnsStuck, bool setOffSinceStop, uint32_t turnsPerSecond);
} // namespace openblack::creature_turn_order
