/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BoundControls.h"

#include <cstddef>

#include "Input/DirectInputKeys.h"

namespace openblack::gui::bound_controls
{
namespace
{
using control_bindings::Mouse;

/// The game's code of a modifier: the left key of each, whichever side is held
uint8_t ModifierCode(input::Modifier modifier) noexcept
{
	switch (modifier)
	{
	case input::Modifier::Shift:
		return 0x2A;
	case input::Modifier::Ctrl:
		return 0x1D;
	case input::Modifier::Alt:
		return 0x38;
	case input::Modifier::None:
	default:
		return 0;
	}
}

Mouse MouseOf(input::MouseInput mouse) noexcept
{
	switch (mouse)
	{
	case input::MouseInput::LeftButton:
		return Mouse::Left;
	case input::MouseInput::MiddleButton:
		return Mouse::Middle;
	case input::MouseInput::WheelUp:
		return Mouse::WheelUp;
	case input::MouseInput::WheelDown:
		return Mouse::WheelDown;
	case input::MouseInput::RightButton:
		return Mouse::Right;
	case input::MouseInput::None:
	default:
		return Mouse::None;
	}
}
} // namespace

Bindings Current(std::span<const input::KeyBinding> bound)
{
	auto bindings = control_bindings::k_Bindings;
	for (size_t i = 0; i < bindings.size() && i < bound.size(); ++i)
	{
		auto& binding = bindings.at(i);
		const auto& chord = bound[i].key;
		const auto code = chord.has_value() ? input::direct_input::CodeOf(chord->key) : std::nullopt;
		binding.key = code.value_or(0);
		binding.modifier = code.has_value() ? ModifierCode(chord->modifier) : 0;
		binding.mouse = MouseOf(bound[i].mouse);
	}
	return bindings;
}

std::map<uint32_t, player_profiles::KeyBinding> ForProfile(std::span<const control_bindings::Binding> bindings)
{
	std::map<uint32_t, player_profiles::KeyBinding> keys;
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		const auto mouse = bindings[i].mouse;
		keys.emplace(static_cast<uint32_t>(i), player_profiles::KeyBinding {.key = bindings[i].key,
		                                                                    .modifier = bindings[i].modifier,
		                                                                    .mouse = static_cast<uint32_t>(mouse)});
	}
	return keys;
}

Bindings FromProfile(const std::map<uint32_t, player_profiles::KeyBinding>& keys)
{
	auto bindings = control_bindings::k_Bindings;
	for (const auto& [action, kept] : keys)
	{
		if (action >= bindings.size())
		{
			continue;
		}
		auto& binding = bindings.at(action);
		binding.key = static_cast<uint8_t>(kept.key);
		binding.modifier = static_cast<uint8_t>(kept.modifier);
		binding.mouse = kept.mouse <= static_cast<uint32_t>(Mouse::Right) ? static_cast<Mouse>(kept.mouse) : Mouse::None;
	}
	return bindings;
}

} // namespace openblack::gui::bound_controls
