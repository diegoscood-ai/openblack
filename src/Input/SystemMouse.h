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

namespace openblack::input
{
/// What the system says of its mouse, as the game asks when its mouse is made: whether a wheel mouse is installed, and
/// how many buttons the mouse has
struct SystemMouse
{
	bool hasWheel {true};
	int32_t buttons {3};
};

/// The system's mouse now. On Windows, the system's own answers (its wheel-present and button-count metrics, the
/// questions the game asks); elsewhere, where the system can't be asked the same, a three-button wheel mouse
[[nodiscard]] SystemMouse QuerySystemMouse() noexcept;

/// The mouse row of the tooltips' mouse pictures: 2 with a wheel, 1 with more than two buttons, else 0
[[nodiscard]] constexpr int32_t MouseRowOf(SystemMouse mouse) noexcept
{
	if (mouse.hasWheel)
	{
		return 2;
	}
	return mouse.buttons > 2 ? 1 : 0;
}

/// A turn of the wheel tells the game the mouse has one, whatever the system said
[[nodiscard]] constexpr SystemMouse AfterWheelTurn(SystemMouse mouse) noexcept
{
	mouse.hasWheel = true;
	return mouse;
}
} // namespace openblack::input
