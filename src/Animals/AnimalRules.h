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

} // namespace openblack::animals
