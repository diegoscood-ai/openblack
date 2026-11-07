/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>
#include <string_view>

// The control icon a help text's $M<n> code asks for: the help system shows the key or the mouse button the player has
// bound to the action, as one of its input prompt icons (help::input_prompt), at the right of the dialogue box
namespace openblack::help::control_icon
{

/// The actions from 6 to 15 are shown as keys; any other as the mouse button bound to them
constexpr int k_FirstKeyAction = 6;
constexpr int k_LastKeyAction = 15;
/// A key icon fades in over this long, in seconds of real time
constexpr float k_KeyFadeSeconds = 0.3f;

/// Whether the action's icon is a key, not a mouse button
[[nodiscard]] constexpr bool IsKeyAction(int action)
{
	return action >= k_FirstKeyAction && action <= k_LastKeyAction;
}

/// The row of a key icon: the box's top plus half the icon's size (both cut towards zero). A mouse icon sits on the
/// click cue's row instead (click_cue::MiddleRow)
[[nodiscard]] int KeyIconRow(int top, int bottom);

/// Adds a key's name to a key icon's text, as the original builds it: the second name follows " + ", every later one
/// " / ". `count` is the number of names the text holds, raised by one
void AppendKeyName(std::u16string& text, int& count, std::u16string_view name);

} // namespace openblack::help::control_icon
