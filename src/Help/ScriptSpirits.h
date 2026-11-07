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

#include <functional>

#include <glm/vec2.hpp>

/// How the scripts name the advisors: the rules between a script's spirit and the advisor it means
namespace openblack::help::script_spirits
{

/// The advisors as the help names them: 1 the good one, 2 the evil one
constexpr int32_t k_GoodSpirit = 1;
constexpr int32_t k_EvilSpirit = 2;

/// An alignment from -1 (evil) to 1 (good) in seven steps, 0 the most evil to 6 the most good: (a + 1) / 2 times the
/// float just below 7, capped at 6, then truncated as the game converts a float (map_coords::FtoL: not a number and far
/// below the range give the integer indefinite, INT32_MIN).
[[nodiscard]] int32_t DiscreteAlignment(float alignment);

/// The advisor a script's spirit means: none or good (0, 1) the good one, evil (2) the evil one, the player's alignment
/// (3) the evil one for an alignment below neutral, its opposite (4) the evil one from neutral up, random (5) either,
/// by a number below 100 that `rand100` draws only then
[[nodiscard]] int32_t HelpSpiritOf(int32_t scriptSpirit, int32_t discreteAlignment, const std::function<uint32_t()>& rand100);

/// The advisor who says a help text, by its narrator: 1 good, 2 evil, 0 neither
[[nodiscard]] int32_t SpiritWhoTalks(int32_t narrator);

/// A script's fraction across or down the screen, from 0 to 1. Not a number is not one.
[[nodiscard]] bool IsScreenFraction(float value);

/// The pixel a script's fractions across and down name on a screen of that width and height: each side times its
/// fraction, truncated
[[nodiscard]] glm::ivec2 ScreenPixel(int32_t width, int32_t height, float x, float y);
/// SPIRIT_POINT_POS and SPIRIT_POINT_GAME_THING: the value a script gives for "in the world", as it is popped. Only 1
/// points in the world; nought and every other value point near the screen
[[nodiscard]] bool PointsInTheWorld(uint32_t inWorld);

} // namespace openblack::help::script_spirits
