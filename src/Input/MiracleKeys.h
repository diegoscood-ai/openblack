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
#include <span>

#include <SDL_scancode.h>

#include "KeyBindings.h"
#include "SpeedKeys.h"

namespace openblack::input
{
/// A key-down of the given key, with any one modifier, that no action bound to that key with that modifier takes
[[nodiscard]] inline bool UnboundKeyDown(SDL_Scancode key, SDL_Scancode wanted, uint16_t sdlModifiers,
                                         std::span<const KeyBinding> bindings)
{
	if (key != wanted)
	{
		return false;
	}
	const auto modifier = HeldModifierOf(sdlModifiers);
	return std::ranges::none_of(bindings, [key, modifier](const KeyBinding& binding) {
		return binding.key.has_value() && CanonicalKey(binding.key->key) == key && binding.key->modifier == modifier;
	});
}

/// A key-down asks to repeat the last miracle: R, with any one modifier, unless R with that modifier is bound to an
/// action, which takes the key instead
[[nodiscard]] inline bool RepeatMiracleKey(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings)
{
	return UnboundKeyDown(key, SDL_SCANCODE_R, sdlModifiers, bindings);
}

/// A key-down asks to open the player's miracle selection: M, with any one modifier, unless M with that modifier is
/// bound to an action, which takes the key instead
[[nodiscard]] inline bool SelectMiracleKey(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings)
{
	return UnboundKeyDown(key, SDL_SCANCODE_M, sdlModifiers, bindings);
}
} // namespace openblack::input
