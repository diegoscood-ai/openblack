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
#include <array>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <SDL_events.h>

#include "Input/KeyBindings.h"

namespace openblack::testbed_scenarios
{

/// The keys a scenario has put down and not yet let go, so that the runner lets them all go when the scenario stops
/// and none is still held in the next scenario of a list
class HeldKeys
{
public:
	/// A key put down: false when it already was
	bool Down(uint32_t key)
	{
		if (IsDown(key))
		{
			return false;
		}
		_keys.push_back(key);
		return true;
	}
	/// A key let go: false when it was not down
	bool Up(uint32_t key)
	{
		const auto found = std::ranges::find(_keys, key);
		if (found == _keys.end())
		{
			return false;
		}
		_keys.erase(found);
		return true;
	}
	[[nodiscard]] bool IsDown(uint32_t key) const { return std::ranges::find(_keys, key) != _keys.end(); }
	/// The keys still down, in the order they went down, and none held any more
	[[nodiscard]] std::vector<uint32_t> TakeAll() { return std::exchange(_keys, {}); }

private:
	std::vector<uint32_t> _keys;
};

/// The cursor keys' actions, by a camera keys command's value: left, right, up and down
inline constexpr std::array<input::BindableActionMap, 4> k_CursorKeyActions {
    input::BindableActionMap::MOVE_LEFT, input::BindableActionMap::MOVE_RIGHT, input::BindableActionMap::MOVE_FORWARDS,
    input::BindableActionMap::MOVE_BACKWARDS};

/// The key an action is bound to with no modifier, as the player presses it; none when it has no such key
[[nodiscard]] inline std::optional<SDL_Scancode> PlainKeyOf(std::span<const input::KeyBinding> bindings,
                                                            input::BindableActionMap action)
{
	const auto found = std::ranges::find(bindings, action, &input::KeyBinding::action);
	if (found == bindings.end() || !found->key.has_value() || found->key->modifier != input::Modifier::None)
	{
		return std::nullopt;
	}
	return found->key->key;
}

/// The two keys held for Creature Mode's camera keys, in the order they go down: Ctrl (zoom on) or Shift (turn on),
/// then the cursor key of the direction (left, right, up or down), as the game's bindings name them. None when either
/// has no key of its own or the direction is not one of the four
[[nodiscard]] inline std::optional<std::array<SDL_Scancode, 2>> CameraKeysOf(std::span<const input::KeyBinding> bindings,
                                                                             size_t direction, bool ctrl)
{
	if (direction >= k_CursorKeyActions.size())
	{
		return std::nullopt;
	}
	const auto modifier = PlainKeyOf(bindings, ctrl ? input::BindableActionMap::ZOOM_ON : input::BindableActionMap::ROTATE_ON);
	const auto cursor = PlainKeyOf(bindings, k_CursorKeyActions.at(direction));
	if (!modifier.has_value() || !cursor.has_value())
	{
		return std::nullopt;
	}
	return std::array {*modifier, *cursor};
}

/// The window's event for the second click of a double click with the left (Move) button, going down or up, which is
/// how the game's actions take a double click
[[nodiscard]] inline SDL_Event DoubleClickEvent(bool down, int32_t x, int32_t y, uint32_t windowId)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.windowID = windowId;
	event.button.button = SDL_BUTTON_LEFT;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 2;
	event.button.x = x;
	event.button.y = y;
	return event;
}

/// A single click's button going down or up (value: 1 left, 2 middle, 3 right), as the window sends the game's input
[[nodiscard]] inline SDL_Event ButtonEvent(bool down, size_t value, int32_t x, int32_t y, uint32_t windowId)
{
	SDL_Event event {};
	event.type = down ? SDL_MOUSEBUTTONDOWN : SDL_MOUSEBUTTONUP;
	event.button.windowID = windowId;
	event.button.button = value == 3 ? SDL_BUTTON_RIGHT : value == 2 ? SDL_BUTTON_MIDDLE : SDL_BUTTON_LEFT;
	event.button.state = down ? SDL_PRESSED : SDL_RELEASED;
	event.button.clicks = 1;
	event.button.x = x;
	event.button.y = y;
	return event;
}

} // namespace openblack::testbed_scenarios
