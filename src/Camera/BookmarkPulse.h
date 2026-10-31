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

#include <glm/vec2.hpp>

/// The bookmarks' pulse: one clock that every bookmark marker on the land shares. Once a frame that draws the land,
/// its time moves on by the frame's game ms (so it stands still while the game is paused), back into the period
/// when it passes it, and gives the markers their width and height: |cos| of the time's turn round the period, plus a
/// minimum, the height a fixed lead ahead of the width.
namespace openblack::bookmark_pulse
{

/// The pulse's period in ms, which the time is kept within
inline constexpr int32_t k_PeriodMs = 2000;
/// How far ahead of the width the height runs, in ms
inline constexpr int32_t k_HeightLeadMs = 200;
/// The turns of the wave per ms (the period of |cos| is 1000 ms)
inline constexpr float k_TurnsPerMs = 0.0005f;
/// A whole turn, in radians, as a float
inline constexpr float k_TwoPi = 6.28318548f;
/// The smallest width and height
inline constexpr float k_Minimum = 0.2f;
/// The smallest half width a marker is drawn with
inline constexpr float k_SmallestSize = 0.0001f;

/// The shared clock and what it gives the markers. Before its first step the width and height are 0
struct Pulse
{
	int32_t timeMs {0};
	float width {0.0f};
	float height {0.0f};
};

/// |cos(time x k_TurnsPerMs x k_TwoPi)| + k_Minimum, each product rounded to a float, the sum rounded once
[[nodiscard]] float Wave(int32_t timeMs);

/// The pulse after a drawn frame of frameMs game ms: the time plus the frame's ms, taken back into the period with the
/// remainder when it is past it (a time of exactly the period stays), then the width at the time and the height at
/// (time + k_HeightLeadMs) within the period
[[nodiscard]] Pulse Step(const Pulse& pulse, uint32_t frameMs);

/// A bookmark marker's half width and half height for the pulse: the width (at least k_SmallestSize) and the marker's
/// stretch height / width times that half width
[[nodiscard]] glm::vec2 MarkerHalfSize(const Pulse& pulse);

} // namespace openblack::bookmark_pulse
