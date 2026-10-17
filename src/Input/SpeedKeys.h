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

#include <algorithm>
#include <optional>
#include <span>

#include <SDL_keyboard.h>

#include "KeyBindings.h"

namespace openblack::input
{
/// The one modifier the game's key handling passes on with a key: Shift first, then Ctrl, then Alt, either side
[[nodiscard]] constexpr Modifier HeldModifierOf(uint16_t sdlModifiers)
{
	if ((sdlModifiers & KMOD_SHIFT) != 0)
	{
		return Modifier::Shift;
	}
	if ((sdlModifiers & KMOD_CTRL) != 0)
	{
		return Modifier::Ctrl;
	}
	if ((sdlModifiers & KMOD_ALT) != 0)
	{
		return Modifier::Alt;
	}
	return Modifier::None;
}

/// Alt+1: half the game speed, but not below 0.5 (a speed whose half is not a number stays not a number)
[[nodiscard]] constexpr float SlowerGameSpeed(float speed)
{
	const float half = speed * 0.5f;
	return half < 0.5f ? 0.5f : half;
}

/// Alt+2: twice the game speed, but not above 2 (a speed that is not a number becomes 2)
[[nodiscard]] constexpr float FasterGameSpeed(float speed)
{
	const float twice = speed + speed;
	return twice <= 2.0f ? twice : 2.0f;
}

/// The game speed a key-down sets: Alt (and no Shift or Ctrl) with 1 slows the game and with 2 speeds it up, unless
/// that key and Alt are bound to an action, which takes the key instead. None for any other key
[[nodiscard]] inline std::optional<float> SpeedForKey(SDL_Scancode key, uint16_t sdlModifiers,
                                                      std::span<const KeyBinding> bindings, float speed)
{
	if (HeldModifierOf(sdlModifiers) != Modifier::Alt || (key != SDL_SCANCODE_1 && key != SDL_SCANCODE_2))
	{
		return std::nullopt;
	}
	const bool bound = std::ranges::any_of(bindings, [key](const KeyBinding& binding) {
		return binding.key.has_value() && CanonicalKey(binding.key->key) == key && binding.key->modifier == Modifier::Alt;
	});
	if (bound)
	{
		return std::nullopt;
	}
	return key == SDL_SCANCODE_1 ? SlowerGameSpeed(speed) : FasterGameSpeed(speed);
}
} // namespace openblack::input
