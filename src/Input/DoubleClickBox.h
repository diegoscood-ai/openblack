/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>

// The game takes its double clicks from the system: a second press within the system's double-click time and inside
// its double-click box, a rectangle of the system's width and height round the first press. SDL counts a second
// press as a double click when it is within its own radius of the first on both axes, so the box becomes that radius.
namespace openblack::input::double_click
{

/// The system's double-click box on a system that keeps none: Windows' default, 4 by 4 pixels
inline constexpr int k_DefaultBoxSize = 4;

/// SDL's double-click radius for the system's box: half its narrower side, as both axes share one radius
[[nodiscard]] constexpr int RadiusForBox(int width, int height)
{
	return std::max(0, std::min(width, height) / 2);
}

} // namespace openblack::input::double_click
