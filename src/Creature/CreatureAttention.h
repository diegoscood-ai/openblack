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

/// How much attention a god pays its creature: every game turn the creature lives, it notes whether its player's camera
/// is looking at it, and the share of its life it was looked at is the attention its mind scroll tells of. Everything
/// here is pure.
namespace openblack::creature_attention
{

/// The camera is looking at a creature closer to it than this
inline constexpr float k_LookingRange = 400.0f;
/// and within this angle of the way it looks: two thirds of seventy degrees
inline constexpr float k_LookingAngle = 1.2217305f * 0.6666667f;

/// Whether a camera at `origin` looking towards `focus` is looking at a creature standing at `creature`: the creature
/// is nearer than the range and within the angle of the way the camera looks. A creature right at the camera, or a
/// camera looking nowhere, is not looked at.
[[nodiscard]] bool IsLookingAt(const glm::vec3& origin, const glm::vec3& focus, const glm::vec3& creature);

/// The share of the turns a creature has lived that its player looked at it, 0 before it has lived a turn. The game
/// divides at a float's precision, the lived turns counted as a signed number; working it out in double first and
/// rounding to a float afterwards gives the same share while the lived turns stay under 2^28.
[[nodiscard]] double Share(uint32_t turnsLookedAt, uint32_t turnsLived);

/// The five words for how much attention, as a number from 0 (the least) to 4: a fifth of the share each, the share
/// times five rounded to a float's precision and cut toward zero, the most for all from four fifths up (and, as the
/// game counts without a sign, for a share below nothing)
[[nodiscard]] int32_t HowMuch(double share);

/// A turn the creature lives out of the hand: looked at, it counts one more turn looked at and its turns in a row not
/// looked at start again from none; not looked at, it counts one more of those
void NoteTurn(bool lookedAt, uint32_t& turnsLookedAt, uint32_t& turnsNotLookedAt);

/// The whole seconds of game time in a number of turns, at whole turns a second (1000 divided by the ms of a turn, cut
/// to a whole number). None when a turn lasts more than a second, or no time, where the game's own division fails
[[nodiscard]] uint32_t WholeSeconds(uint32_t turns, uint32_t msPerTurn);

/// How much it wants its player's attention from loneliness (desire source 36): the whole seconds in a row it has not
/// been looked at, fully after a minute
[[nodiscard]] float Loneliness(uint32_t turnsNotLookedAt, uint32_t msPerTurn);

/// How much it wants its player's attention from lack of anything to do with the player (desire source 37): the whole
/// seconds since its player last sent it to act on something, fully after two minutes
[[nodiscard]] float LackOfInteraction(uint32_t turnsSinceMadeToAct, uint32_t msPerTurn);

/// Whether its player is ignoring it: more turns in a row not looked at than two minutes hold, at whole turns a
/// second. With no time a turn, where the game's own division fails, never
[[nodiscard]] bool IgnoredByPlayer(uint32_t turnsNotLookedAt, uint32_t msPerTurn);

} // namespace openblack::creature_attention
