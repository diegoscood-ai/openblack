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

/// What the help system's interface pass answers when the click or the key was used by the text (it cut the text short)
constexpr int k_ClickTaken = 0x14;

/// Whether the help system's answer takes the click from the hand. The interface's pass asks the help system after
/// the boxes; on this answer it ends the hand's action, resets the action state and clears the buttons it latched, and
/// the state's own process does not run for that pass. Any other answer leaves the click to the hand
[[nodiscard]] constexpr bool TakesClickFromHand(int answer)
{
	return answer == k_ClickTaken;
}

} // namespace openblack::help::dialogue_click
