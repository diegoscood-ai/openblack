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

/// The rules of the land's animals that are pure arithmetic
namespace openblack::animals
{

/// A dead body that has lain long enough goes in a grey puff of smoke of this size, half its height out from where it
/// lies along the way it faces
inline constexpr float k_DeadBodySmokeSize = 1.0f;
[[nodiscard]] glm::vec3 DeadBodySmokePlace(const glm::vec3& position, float heading, float height);

/// How many cells a grazer's food search goes round: a tenth of its domain's radius, squared, in floats, truncated
[[nodiscard]] int32_t GrazeSearchCells(uint16_t domainRadius);

/// Whether an animal is far enough from a slow predator for its fleeing to count for the turns before it reacts to it
/// again: its true distance at least the reaction's minimum; a distance that is not a number is not
[[nodiscard]] bool FarEnoughFromPredator(float distance, float minimum);

/// Whether an animal that follows one reaction weighs another it is offered: one of another type always; one of the
/// same type only where the type table lets a living switch within the type (`sameTypeSwitch`), and never the very
/// reaction it follows
[[nodiscard]] bool MayWeighAnotherReaction(uint8_t currentType, uint32_t currentId, uint8_t newType, uint32_t newId,
                                           bool sameTypeSwitch);

/// How far an animal looking at a thing it watches turns in a turn, in 2048ths of a circle
inline constexpr int32_t k_LookAtObjectTurn = 0x80;

/// One turn of looking at a thing: the heading `current` (2048ths) goes straight to `target` when they are less than
/// `step` apart, else `step` towards it the short way, wrapped to the circle (exactly half a circle apart, down when
/// the target is above it and up when below)
[[nodiscard]] uint16_t LookTowards(uint16_t current, uint16_t target, int32_t step);

/// Whether an animal lets go of what it was eating at the start of its state's turn: the food is no longer available,
/// or the player's hand holds it
[[nodiscard]] constexpr bool DropsFoodTarget(bool available, bool inPlayersHand)
{
	return !available || inPlayersHand;
}

} // namespace openblack::animals
