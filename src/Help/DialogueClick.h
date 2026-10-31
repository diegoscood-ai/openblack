/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Input/MouseButtons.h"

/// Which of the mouse's button events gives the help system its click on the dialogue's text
namespace openblack::help::dialogue_click
{

/// The left button let go, as the original's interface sets its click on the left binding's release
[[nodiscard]] constexpr bool IsClick(input::MouseButton button, bool down)
{
	return button == input::MouseButton::Left && !down;
}

} // namespace openblack::help::dialogue_click
