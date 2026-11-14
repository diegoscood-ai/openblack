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
/// (KeyTakenByBinding)
[[nodiscard]] inline bool UnboundKeyDown(SDL_Scancode key, SDL_Scancode wanted, uint16_t sdlModifiers,
                                         std::span<const KeyBinding> bindings, bool scriptWideScreen)
{
	return key == wanted && !KeyTakenByBinding(key, sdlModifiers, bindings, scriptWideScreen);
}

/// A key-down asks to repeat the last miracle: R, with any one modifier, unless R with that modifier is bound to an
/// action that takes the key instead
[[nodiscard]] inline bool RepeatMiracleKey(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings,
                                           bool scriptWideScreen = false)
{
	return UnboundKeyDown(key, SDL_SCANCODE_R, sdlModifiers, bindings, scriptWideScreen);
}

/// A key-down asks to open the player's miracle selection: M, with any one modifier, unless M with that modifier is
/// bound to an action that takes the key instead
[[nodiscard]] inline bool SelectMiracleKey(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings,
                                           bool scriptWideScreen = false)
{
	return UnboundKeyDown(key, SDL_SCANCODE_M, sdlModifiers, bindings, scriptWideScreen);
}
} // namespace openblack::input
