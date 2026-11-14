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
#include <span>

#include <glm/vec2.hpp>

#include "Enums.h"

// The swirling vortex between the lands: when it opens and closes, the glow it lays on the ground, the hole it opens
// in the land and how deep its swirl sits under the land. A vortex fades in (2 seconds of nothing, then 5 seconds
// easing open) or starts fully open, and when told to fade out eases shut over 5 seconds and is gone 2 seconds later.
// While it opens, the land vortices (not the volcano's mouth) pull an 11 by 11 cell square of the land towards the
// square's average height, wholly within 50 of the middle and fading to nothing at 56. The levelling only ever grows and
// is never undone. Every step is a float operation, rounded once, in the original's order. Pure rules, tested without
// the game.

namespace openblack::vortex
{

/// How far a vortex pulls things in, whatever its kind
inline constexpr float k_PullRadius = 50.0f;
/// A fading vortex waits this long before it starts to change
inline constexpr float k_FadeDelaySeconds = 2.0f;
/// Then it eases open or shut over this long
inline constexpr float k_FadeSeconds = 5.0f;
/// A fade is over, and the vortex fully open or gone, once more than this much time has passed
inline constexpr float k_FadeOverSeconds = k_FadeDelaySeconds + k_FadeSeconds;

/// The levelled ground's reach: the land is pulled fully to the average within the inner radius, less and less out to
/// the outer one, and not at all beyond it
inline constexpr float k_LevelReach = 58.0f;
inline constexpr float k_LevelFullRadius = 50.0f;
inline constexpr float k_LevelOuterRadius = k_LevelReach - 2.0f;
/// The levelled square: cells either side of the vortex's own cell, and along each side
inline constexpr int k_LevelHalfSide = 5;
inline constexpr int k_LevelSide = k_LevelHalfSide * 2 + 1;
inline constexpr size_t k_LevelCellCount = static_cast<size_t>(k_LevelSide) * k_LevelSide;

/// The hole a vortex opens in the land never takes in the most opaque part of its texture
inline constexpr uint8_t k_GroundHoleMaxThreshold = 235;

/// How open a vortex is after some seconds in a state, 0 to 1, eased in and out: inactive 0, active 1; fading in, 0
/// for 2 seconds and then eased up over 5; fading out (or any other state), eased down over 5 seconds and then 0. The
/// ease is ((3 - 2x) x) x
[[nodiscard]] float Openness(VortexStateType state, float seconds);

/// How far the levelling has gone, 0 to 1: the openness eased out (one minus the square of what is left), and complete
/// once the vortex is fading out
[[nodiscard]] float LevelAmount(VortexStateType state, float seconds);

/// The glow the vortex lays on the ground, 0 to 1: inactive 0, active 0.6; rising to full over the delay of a fade-in
/// and settling to 0.6 as it opens; rising back to full as it fades out, then dying away over the last 2 seconds
[[nodiscard]] float GlowBrightness(VortexStateType state, float seconds);

/// How opaque the hole texture must be, 0 to 255, for the land to be drawn there: the openness in 255ths, truncated,
/// and never more than 235 (compared unsigned, so a negative product is capped too)
[[nodiscard]] uint8_t GroundHoleThreshold(float openness);

/// How far below the ground the swirl drawn under the land sits: 2.5 while the levelling hasn't begun, rising to 0.3
[[nodiscard]] float SwirlDepth(float levelAmount);

/// Whether a kind of vortex levels the ground: the land vortices do, the volcano's mouth does not
[[nodiscard]] constexpr bool LevelsGround(VortexType type)
{
	return type != VortexType::Volcano;
}

/// The map cell a vortex at a point on the land levels round: its position in cells, rounded towards zero
[[nodiscard]] glm::ivec2 CentreCell(glm::vec2 centre);

/// The cells of the levelled square, x major: the first cell's x and z, then along z for each x
[[nodiscard]] glm::ivec2 SquareCell(glm::ivec2 centreCell, size_t index);

/// The square's average height: every cell counts, cells off the land as 0
[[nodiscard]] float AverageAltitude(std::span<const uint8_t> altitudes);

/// How much of the way to the average a cell at a distance from the middle is pulled, before the levelling's amount.
/// A helper only: the levelled height is not built from it, since the game rounds its own steps differently
[[nodiscard]] float LevelWeight(float distance);

/// The levelled height of a cell that was at an altitude when the vortex first opened, at a distance from the middle
[[nodiscard]] uint8_t LevelledAltitude(uint8_t original, float average, float distance, float amount);

/// The square's levelled heights, x major as its cells: each cell from its height when the vortex first opened, the
/// square's average, its distance from the vortex's middle and the levelling's amount
[[nodiscard]] std::array<uint8_t, k_LevelCellCount> LevelSquare(glm::vec2 centre, std::span<const uint8_t> originals,
                                                                float average, float amount);

} // namespace openblack::vortex
