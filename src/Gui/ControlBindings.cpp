/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControlBindings.h"

#include "TextDatabase.h"

using namespace openblack::gui;

/// GetKeyNameText's names, which the game shows, of the DirectInput keys bound by default
std::u16string control_bindings::KeyName(uint8_t key)
{
	switch (key)
	{
	case 0x0B:
		return u"0";
	case 0x10:
		return u"Q";
	case 0x14:
		return u"T";
	case 0x16:
		return u"U";
	case 0x1D:
	case 0x9D:
		return u"CTRL";
	case 0x1E:
		return u"A";
	case 0x1F:
		return u"S";
	case 0x26:
		return u"L";
	case 0x2C:
		return u"Z";
	case 0x2E:
		return u"C";
	case 0x2F:
		return u"V";
	case 0x30:
		return u"B";
	case 0x31:
		return u"N";
	case 0x36:
		return u"RIGHT SHIFT";
	case 0x39:
		return u"SPACE";
	case 0xC8:
		return u"UP";
	case 0xCB:
		return u"LEFT";
	case 0xCD:
		return u"RIGHT";
	case 0xD0:
		return u"DOWN";
	default:
		if (key >= 0x3B && key <= 0x44)
		{
			return u"F" + ToUtf16(std::to_string(key - 0x3B + 1));
		}
		return u"?";
	}
}

std::u16string control_bindings::KeyText(const Binding& binding)
{
	if (binding.key == 0)
	{
		return {};
	}
	return binding.modifier != 0 ? KeyName(binding.modifier) + u" + " + KeyName(binding.key) : KeyName(binding.key);
}

std::string_view control_bindings::MouseTextName(Mouse mouse) noexcept
{
	switch (mouse)
	{
	case Mouse::Left:
		return "HELP_TEXT_ACTIONS_LEFTMOUSEBUTTON";
	case Mouse::Right:
		return "HELP_TEXT_ACTIONS_RIGHTMOUSEBUTTON";
	case Mouse::Middle:
		return "HELP_TEXT_ACTIONS_MIDDLEMOUSEBUTTON";
	case Mouse::WheelUp:
		return "HELP_TEXT_ACTIONS_WHEEL_UP";
	case Mouse::WheelDown:
		return "HELP_TEXT_ACTIONS_WHEEL_DOWN";
	default:
		return {};
	}
}
