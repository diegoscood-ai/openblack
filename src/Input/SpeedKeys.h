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

/// Whether a bound action acts on its key going down, which then goes no further (to the miracle, pause, bookmark and
/// jump keys): the help, the zooms to places and to the temple's rooms, the leash keys, talk, the villagers' names and
/// details and the quick save and load. The actions read while held (the hand's buttons, the camera's moves, turns,
/// tilts and zooms and their modifiers) let the key go on
[[nodiscard]] constexpr bool ActsOnKeyDown(BindableActionMap action)
{
	switch (action)
	{
	case BindableActionMap::HELP:
	case BindableActionMap::ZOOM_TO_TEMPLE:
	case BindableActionMap::ZOOM_TO_CREATURE:
	case BindableActionMap::ZOOM_TO_REALM:
	case BindableActionMap::ZOOM_TO_INSIDE_TEMPLE:
	case BindableActionMap::ZOOM_TO_CREATURE_ROOM:
	case BindableActionMap::ZOOM_TO_CHALLENGE_ROOM:
	case BindableActionMap::ZOOM_TO_SAVE_GAME_ROOM:
	case BindableActionMap::ZOOM_TO_OPTIONS_ROOM:
	case BindableActionMap::ZOOM_TO_LIBRARY:
	case BindableActionMap::LEASH_UNLEASH_CREATURE:
	case BindableActionMap::TALK:
	case BindableActionMap::SHOW_VILLAGER_NAMES:
	case BindableActionMap::SHOW_VILLAGER_DETAILS:
	case BindableActionMap::QUICK_SAVE:
	case BindableActionMap::QUICK_LOAD:
	case BindableActionMap::NEXT_LEASH:
	case BindableActionMap::PREVIOUS_LEASH:
		return true;
	default:
		return false;
	}
}

/// Whether the action bound to a key with the held modifier (the first one bound to it) takes the key down: it acts on
/// the key going down, and no script holds the cinema bars, under which no action acts and the key goes on
[[nodiscard]] inline bool KeyTakenByBinding(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings,
                                            bool scriptWideScreen)
{
	const auto modifier = HeldModifierOf(sdlModifiers);
	const auto bound = std::ranges::find_if(bindings, [key, modifier](const KeyBinding& binding) {
		return binding.key.has_value() && CanonicalKey(binding.key->key) == key && binding.key->modifier == modifier;
	});
	return bound != bindings.end() && !scriptWideScreen && ActsOnKeyDown(bound->action);
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

/// A key-down pauses or unpauses the game: P with no Shift, Ctrl or Alt held, unless P alone is bound to an action
/// that takes the key (KeyTakenByBinding)
[[nodiscard]] inline bool PauseKey(SDL_Scancode key, uint16_t sdlModifiers, std::span<const KeyBinding> bindings,
                                   bool scriptWideScreen = false)
{
	if (key != SDL_SCANCODE_P || HeldModifierOf(sdlModifiers) != Modifier::None)
	{
		return false;
	}
	return !KeyTakenByBinding(key, sdlModifiers, bindings, scriptWideScreen);
}
} // namespace openblack::input
