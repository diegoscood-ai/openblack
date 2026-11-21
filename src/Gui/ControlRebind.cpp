/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ControlRebind.h"

#include "Input/DirectInputKeys.h"

namespace openblack::gui::control_rebind
{

bool IsBindableKey(uint8_t key) noexcept
{
	switch (key)
	{
	case 0x01: // Escape
	case 0x0F: // Tab
	case 0x19: // P
	case 0x3B: // F1
	case 0x3C: // F2
	case 0x9C: // the keypad's Enter
		return false;
	default:
		return true;
	}
}

uint8_t ModifierWith(uint8_t key, bool shift, bool ctrl, bool alt) noexcept
{
	if (shift)
	{
		return key == k_Shift || key == 0x36 ? 0 : k_Shift;
	}
	if (ctrl)
	{
		return key == k_Ctrl || key == 0x9D ? 0 : k_Ctrl;
	}
	if (alt)
	{
		return key == k_Alt || key == 0xB8 ? 0 : k_Alt;
	}
	return 0;
}

void AssignKey(std::span<control_bindings::Binding> bindings, size_t action, uint8_t key, uint8_t modifier) noexcept
{
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		if (i != action && bindings[i].key == key && bindings[i].modifier == modifier)
		{
			bindings[i].key = 0;
			bindings[i].modifier = 0;
		}
	}
	if (action < bindings.size())
	{
		bindings[action].key = key;
		bindings[action].modifier = modifier;
	}
}

void AssignMouse(std::span<control_bindings::Binding> bindings, size_t action, control_bindings::Mouse mouse) noexcept
{
	for (auto& binding : bindings)
	{
		if (binding.mouse == mouse)
		{
			binding.mouse = control_bindings::Mouse::None;
		}
	}
	if (action < bindings.size())
	{
		bindings[action].mouse = mouse;
	}
}

std::optional<input::KeyChord> ChordOf(const control_bindings::Binding& binding) noexcept
{
	const auto key = binding.key != 0 ? input::direct_input::ScancodeOf(binding.key) : std::nullopt;
	if (!key)
	{
		return std::nullopt;
	}
	auto modifier = input::Modifier::None;
	switch (binding.modifier)
	{
	case k_Shift:
		modifier = input::Modifier::Shift;
		break;
	case k_Ctrl:
		modifier = input::Modifier::Ctrl;
		break;
	case k_Alt:
		modifier = input::Modifier::Alt;
		break;
	default:
		break;
	}
	return input::KeyChord {.key = *key, .modifier = modifier};
}

input::MouseInput MouseInputOf(control_bindings::Mouse mouse) noexcept
{
	using control_bindings::Mouse;
	switch (mouse)
	{
	case Mouse::Left:
		return input::MouseInput::LeftButton;
	case Mouse::Middle:
		return input::MouseInput::MiddleButton;
	case Mouse::WheelUp:
		return input::MouseInput::WheelUp;
	case Mouse::WheelDown:
		return input::MouseInput::WheelDown;
	case Mouse::Right:
		return input::MouseInput::RightButton;
	default:
		return input::MouseInput::None;
	}
}

} // namespace openblack::gui::control_rebind
