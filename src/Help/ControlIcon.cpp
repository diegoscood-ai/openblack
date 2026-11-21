/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControlIcon.h"

#include "ClickCue.h"

namespace openblack::help::control_icon
{

std::optional<SDL_Scancode> PrimaryKey(std::span<const input::KeyBinding> bindings, int action)
{
	// the action's row in the table, found by its bit
	if (action < 0 || action >= static_cast<int>(input::k_KeyBindingCount))
	{
		return std::nullopt;
	}
	const auto index =
	    input::IndexOf(bindings, static_cast<input::BindableActionMap>(uint64_t {1} << static_cast<unsigned>(action)));
	if (!index.has_value())
	{
		return std::nullopt;
	}
	const auto& key = bindings[*index].key;
	if (!key.has_value() || key->key == SDL_SCANCODE_UNKNOWN)
	{
		return std::nullopt;
	}
	return key->key;
}

int KeyIconRow(int top, int bottom)
{
	return top + click_cue::CueSize(top, bottom) / 2;
}

void AppendKeyName(std::u16string& text, int& count, std::u16string_view name)
{
	++count;
	if (count == 2)
	{
		text += u" + ";
	}
	else if (count > 2)
	{
		text += u" / ";
	}
	text += name;
}

} // namespace openblack::help::control_icon
