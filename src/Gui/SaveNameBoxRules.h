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

/// The keys of the save game room's name box. See docs/wiki/interface/temple-interior.md#the-save-game-room
namespace openblack::gui::save_name_box
{

/// What a key pressed in the name box does
enum class KeyStep : uint8_t
{
	/// The key goes to the control with the focus, as the name's box's typing and caret keys
	ToControl,
	/// The focus goes from the name's box to OK
	FocusOk,
	/// OK: the game is saved under the name
	Save,
	/// Cancel: the box closes and nothing is saved
	Cancel,
};

/// Return in the name's box gives the focus to OK, and Return anywhere else is OK; Escape is Cancel
[[nodiscard]] constexpr KeyStep StepOfKey(bool returnKey, bool escapeKey, bool nameHasFocus) noexcept
{
	if (returnKey)
	{
		return nameHasFocus ? KeyStep::FocusOk : KeyStep::Save;
	}
	return escapeKey ? KeyStep::Cancel : KeyStep::ToControl;
}

} // namespace openblack::gui::save_name_box
