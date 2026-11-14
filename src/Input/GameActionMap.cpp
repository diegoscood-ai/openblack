/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "GameActionMap.h"

#include <algorithm>
#include <utility>

#include <SDL_events.h>
#include <glm/common.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "DoubleClickBox.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "FixedMouse.h"
#include "Game.h"
#include "GameCursor.h"
#include "Hand/HandDemo.h"
#include "Input/InputReplay.h"
#include "Locator.h"
#include "RealInput.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack::input;

bool openblack::input::UsesRealInput()
{
	return !FixedMouse().has_value() && !IgnoreRealInput() && MouseAt() == nullptr && !replay::IsPlaying();
}

namespace
{
/// Whether the run takes the player's own input, live or from a recording playing: then a letting go that went elsewhere
/// (to a box, the menu or a debug window) is made up from what the window's events held, which a recording gives back
[[nodiscard]] bool PlaysPlayersInput()
{
	return UsesRealInput() || replay::IsPlaying();
}

/// Puts the real pointer at a spot in the game's window, and gives how far it jumped: the window reports the jump as
/// the pointer moving, with the next events, which isn't the mouse moving
glm::ivec2 WarpRealPointer(glm::ivec2 position)
{
	if (!openblack::Locator::windowing::has_value())
	{
		return glm::ivec2(0);
	}
	auto* window = static_cast<SDL_Window*>(openblack::Locator::windowing::value().GetHandle());
	if (window == nullptr)
	{
		return glm::ivec2(0);
	}
	glm::ivec2 before {0};
	SDL_GetMouseState(&before.x, &before.y);
	SDL_WarpMouseInWindow(window, position.x, position.y);
	glm::ivec2 after {0};
	SDL_GetMouseState(&after.x, &after.y);
	return after - before;
}
} // namespace

GameActionMap::GameActionMap()
{
	ApplyMouseBindings();
}

SystemMouse GameActionMap::GetSystemMouse() const
{
	// the test runs read no real input, the system's mouse included: its answers can change while the machine runs
	return MouseOfRun(_systemMouse, UsesRealInput(), _wheelTurned);
}

void GameActionMap::ApplyMouseBindings()
{
	_mouseBindings.clear();
	_mouseModBindings.clear();
	constexpr std::array<std::pair<MouseInput, int>, 3> k_Buttons {{
	    {MouseInput::LeftButton, SDL_BUTTON_LMASK},
	    {MouseInput::MiddleButton, SDL_BUTTON_MMASK},
	    {MouseInput::RightButton, SDL_BUTTON_RMASK},
	}};
	for (const auto& [mouse, mask] : k_Buttons)
	{
		if (const auto actions = ActionsForMouse(_bindings, mouse); actions != BindableActionMap::NONE)
		{
			_mouseBindings.emplace(mask, actions);
		}
	}
	const auto wheel = [this](MouseInput mouse) -> std::optional<BindableActionMap> {
		const auto actions = ActionsForMouse(_bindings, mouse);
		return actions != BindableActionMap::NONE ? std::optional(actions) : std::nullopt;
	};
	_mouseWheelBinding[0] = wheel(MouseInput::WheelUp);
	_mouseWheelBinding[1] = wheel(MouseInput::WheelDown);
}

std::span<const KeyBinding> GameActionMap::GetKeyBindings() const
{
	return _bindings;
}

void GameActionMap::SetKeyBinding(BindableActionMap action, std::optional<KeyChord> key)
{
	if (const auto index = IndexOf(_bindings, action))
	{
		const auto letGo = [this](BindableActionMap released) {
			_bindableMap =
			    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(released));
		};
		// A key held by one action only: every other action bound to the same key and modifier loses it
		if (key.has_value())
		{
			for (auto& other : _bindings)
			{
				if (other.action != action && other.key == key)
				{
					letGo(other.action);
					other.key.reset();
				}
			}
		}
		// Whatever the old key held of the action is let go
		letGo(action);
		_bindings.at(*index).key = key;
	}
}

void GameActionMap::SetMouseBinding(BindableActionMap action, MouseInput mouse)
{
	if (const auto index = IndexOf(_bindings, action))
	{
		// Whatever the old button held of the action is let go
		_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(action));
		_bindings.at(*index).mouse = mouse;
		ApplyMouseBindings();
	}
}

void GameActionMap::ResetKeyBindings()
{
	_bindings = k_DefaultKeyBindings;
	ApplyMouseBindings();
}

void GameActionMap::QueuePress(BindableActionMap action)
{
	_queuedPresses.emplace_back(action);
}

bool GameActionMap::TakeDoubleClick()
{
	const bool marked = GetUnbindable(UnbindableActionMap::DOUBLE_CLICK);
	_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) &
	                                                  ~static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK));
	return marked;
}

bool GameActionMap::HasQueuedPresses() const
{
	return !_queuedPresses.empty() || !_queuedReleases.empty() || _queuedHeld != BindableActionMap::NONE;
}

void GameActionMap::ReleaseKeysNoLongerHeld()
{
	for (size_t key = 0; key < _heldKeys.size(); ++key)
	{
		if (_heldKeys.test(key) && !_windowKeys.test(key))
		{
			SDL_Event letGo {};
			letGo.type = SDL_KEYUP;
			letGo.key.keysym.scancode = static_cast<SDL_Scancode>(key);
			ProcessEvent(letGo);
		}
	}
}

void GameActionMap::ReleaseButtonsNoLongerHeld()
{
	for (uint8_t button = SDL_BUTTON_LEFT; button <= SDL_BUTTON_X2; ++button)
	{
		if ((_currentMouseButtons & SDL_BUTTON(button)) != 0 && (_windowButtons & SDL_BUTTON(button)) == 0)
		{
			SDL_Event letGo {};
			letGo.type = SDL_MOUSEBUTTONUP;
			letGo.button.button = button;
			letGo.button.clicks = 1;
			ProcessEvent(letGo);
		}
	}
}

void GameActionMap::SeeDispatchedEvent(const SDL_Event& event)
{
	switch (event.type)
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		if (const auto key = static_cast<size_t>(event.key.keysym.scancode); key < _windowKeys.size())
		{
			_windowKeys.set(key, event.type == SDL_KEYDOWN);
		}
		// A key's event carries the modifiers held with it; only Shift, Ctrl and Alt count, as in the original's own
		// modifiers, so the lock keys (Num Lock, Caps Lock), which the system may turn on or off unseen, never do
		_windowModifiers = static_cast<uint16_t>(event.key.keysym.mod & k_CountedModifiers);
		break;
	case SDL_MOUSEBUTTONDOWN:
		_windowButtons |= SDL_BUTTON(event.button.button);
		break;
	case SDL_MOUSEBUTTONUP:
		_windowButtons &= ~SDL_BUTTON(event.button.button);
		break;
	default:
		break;
	}
}

glm::ivec2 GameActionMap::PointerPosition() const
{
	// The game's cursor, which the mouse's motion events move as they are dispatched, live as in a recording's playback:
	// the real pointer's own state runs ahead of the events (relative mode drops the motion queued behind a press, and
	// a warp in relative mode sends none), so a recording could not give it back. OPENBLACK_MOUSE_AT's from the first
	// frame on, or the fixed one, take its place in the test runs, and a playing hand demo's is the mouse
	const auto screen = Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(0);
	return FrameCursor(GameCursor(), FixedMouse(), MouseAtPixel(MouseAt(), screen),
	                   Locator::inputState::has_value() && hand_demo::IsPlaying());
}

void GameActionMap::ApplyQueuedPresses()
{
	for (const auto& event : std::exchange(_queuedReleases, {}))
	{
		ProcessEvent(event);
	}
	_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
	                                              ~static_cast<uint64_t>(std::exchange(_queuedHeld, BindableActionMap::NONE)));
	for (const auto action : std::exchange(_queuedPresses, {}))
	{
		const auto index = IndexOf(_bindings, action);
		if (!index.has_value())
		{
			continue;
		}
		const auto& binding = _bindings.at(*index);
		if (binding.key.has_value())
		{
			// The key goes down with its modifier held, through the same lookup as the player's keys
			constexpr std::array<std::pair<Modifier, uint16_t>, 4> k_Modifiers {{
			    {Modifier::None, KMOD_NONE},
			    {Modifier::Ctrl, KMOD_LCTRL},
			    {Modifier::Shift, KMOD_LSHIFT},
			    {Modifier::Alt, KMOD_LALT},
			}};
			SDL_Event press {};
			press.type = SDL_KEYDOWN;
			press.key.keysym.scancode = binding.key->key;
			press.key.keysym.mod =
			    std::ranges::find(k_Modifiers, binding.key->modifier, &std::pair<Modifier, uint16_t>::first)->second;
			ProcessEvent(press);
			auto release = press;
			release.type = SDL_KEYUP;
			_queuedReleases.emplace_back(release);
			// The made up key isn't held on the keyboard, and only its queued release lets it go
			_heldKeys.reset(static_cast<size_t>(binding.key->key));
		}
		else if (binding.mouse == MouseInput::WheelUp || binding.mouse == MouseInput::WheelDown)
		{
			SDL_Event wheel {};
			wheel.type = SDL_MOUSEWHEEL;
			wheel.wheel.y = binding.mouse == MouseInput::WheelUp ? 1 : -1;
			wheel.wheel.preciseY = static_cast<float>(wheel.wheel.y);
			ProcessEvent(wheel);
		}
		else
		{
			// A mouse button or an unbound action is held for the frame
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) | static_cast<uint64_t>(action));
			_queuedHeld = static_cast<BindableActionMap>(static_cast<uint64_t>(_queuedHeld) | static_cast<uint64_t>(action));
		}
	}
}

bool GameActionMap::GetBindable(BindableActionMap action) const
{
	return (static_cast<uint64_t>(_bindableMap) & static_cast<uint64_t>(action) & ~static_cast<uint64_t>(_blocked)) != 0;
}

bool GameActionMap::GetUnbindable(UnbindableActionMap action) const
{
	return (static_cast<uint8_t>(_unbindableMap) & static_cast<uint8_t>(action)) != 0;
}

bool GameActionMap::GetBindableChanged(BindableActionMap action) const
{
	return ((static_cast<uint64_t>(_bindableMap) ^ static_cast<uint64_t>(_bindableMapPrevious)) &
	        static_cast<uint64_t>(action) & ~static_cast<uint64_t>(_blocked)) != 0;
}

bool GameActionMap::GetUnbindableChanged(UnbindableActionMap action) const
{
	return ((static_cast<uint8_t>(_unbindableMap) ^ static_cast<uint8_t>(_unbindableMapPrevious)) &
	        static_cast<uint8_t>(action)) != 0;
}

bool GameActionMap::GetBindableRepeat(BindableActionMap action) const
{
	return ((static_cast<uint64_t>(_bindableMap) & static_cast<uint64_t>(_bindableMapPrevious)) &
	        static_cast<uint64_t>(action) & ~static_cast<uint64_t>(_blocked)) != 0;
}

bool GameActionMap::GetUnbindableRepeat(UnbindableActionMap action) const
{
	return ((static_cast<uint8_t>(_unbindableMap) & static_cast<uint8_t>(_unbindableMapPrevious)) &
	        static_cast<uint8_t>(action)) != 0;
}

glm::uvec2 GameActionMap::GetMousePosition() const
{
	return _mousePosition;
}

glm::ivec2 GameActionMap::GetPointerPosition() const
{
	if (!UsesRealInput())
	{
		return glm::ivec2(_mousePosition);
	}
	glm::ivec2 pointer {0};
	SDL_GetMouseState(&pointer.x, &pointer.y);
	return pointer;
}

glm::ivec2 GameActionMap::GetMouseDelta() const
{
	return _mouseMotion.Delta();
}

float GameActionMap::GetMouseWheelDelta() const
{
	return _mouseWheelDelta;
}

void GameActionMap::WarpCursor(glm::ivec2 position)
{
	_mousePosition = position;
	_cursorWarp = position;
	// The game's cursor goes there too, so the next frames read it there whether or not the real pointer's warp sends
	// a motion (it sends none in relative mode), live as in a recording's playback; not where OPENBLACK_MOUSE_AT or the
	// fixed mouse stand in for it, nor under a playing hand demo, which is the mouse
	if (Locator::inputState::has_value() && !FixedMouse().has_value() && MouseAt() == nullptr && !hand_demo::IsPlaying())
	{
		GameCursorRef() = position;
	}
	if (UsesRealInput())
	{
		_mouseMotion.Put(WarpRealPointer(position));
	}
}

std::optional<glm::ivec2> GameActionMap::GetCursorWarp() const
{
	return _cursorWarp;
}

void GameActionMap::AllowCursorFreeze(bool allowed)
{
	_cursorFreezeAllowed = allowed;
}

bool GameActionMap::IsCursorFrozen() const
{
	return _cursorFreeze.IsFrozen();
}

bool GameActionMap::IsKeyHeld(SDL_Scancode key) const
{
	const auto index = static_cast<size_t>(key);
	return index < _heldKeys.size() && _heldKeys.test(index);
}

bool GameActionMap::IsMouseButtonHeld(MouseButton button) const
{
	uint32_t mask = 0;
	switch (button)
	{
	case MouseButton::Left:
		mask = SDL_BUTTON_LMASK;
		break;
	case MouseButton::Middle:
		mask = SDL_BUTTON_MMASK;
		break;
	case MouseButton::Right:
		mask = SDL_BUTTON_RMASK;
		break;
	case MouseButton::Other:
	default:
		break;
	}
	// The test runs read no button but the ones a recording plays
	return PlaysPlayersInput() && (_windowButtons & mask) != 0;
}

void GameActionMap::Frame()
{
	// The double click and the two buttons together count as changed only on the frame they start or end, so a double
	// click is taken once, not on every frame its button stays down. Kept before the two buttons are read, so that
	// their start shows as a change too
	_unbindableMapPrevious = _unbindableMap;
	_cursorWarp.reset();
	_keyDowns.clear();
	_keyBuffer.Clear();
	if (PlaysPlayersInput())
	{
		ReleaseKeysNoLongerHeld();
		ReleaseButtonsNoLongerHeld();
	}

	constexpr uint32_t k_BothButtons = SDL_BUTTON_LMASK | SDL_BUTTON_RMASK;
	if (auto log = spdlog::get("input"); log != nullptr && (_currentMouseButtons & k_BothButtons) == k_BothButtons)
	{
		log->debug("Both buttons: held by the events, the window's {:#x}", _windowButtons);
	}
	// Both buttons held as their own button events hold them, as the original reads the mouse buttons its window's
	// messages set (in play the same as the pointer's state, whose let-goes elsewhere are made up as events above)
	if ((_currentMouseButtons & k_BothButtons) == k_BothButtons)
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) |
		                                                  static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK));
		const auto leftWithModifier = _mouseModBindings[SDL_BUTTON_LMASK].second;
		const auto rightWithModifier = _mouseModBindings[SDL_BUTTON_RMASK].second;
		const auto left = _mouseBindings[SDL_BUTTON_LMASK];
		const auto right = _mouseBindings[SDL_BUTTON_RMASK];
		_bindableMap = ClearedForBothButtons(_bindableMap, leftWithModifier, rightWithModifier, left, right);
	}
	else
	{
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) &
		                                                  ~static_cast<uint8_t>(UnbindableActionMap::TWO_BUTTON_CLICK));
	}

	if (_bindableMap != BindableActionMap::NONE || _unbindableMap != UnbindableActionMap::NONE)
	{
		SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "GameActionMap:");
	}
#define get_print(x)                                               \
	do                                                             \
	{                                                              \
		if (Get(BindableActionMap::x))                             \
		{                                                          \
			SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "\t{}", #x); \
		}                                                          \
	} while (0)

	get_print(HELP);
	get_print(MOVE);
	get_print(ACTION);
	get_print(ZOOM_OUT);
	get_print(ZOOM_IN);
	get_print(TALK);
	get_print(ZOOM_ON);
	get_print(MOVE_LEFT);
	get_print(MOVE_RIGHT);
	get_print(MOVE_FORWARDS);
	get_print(MOVE_BACKWARDS);
	get_print(TILT_UP);
	get_print(TILT_DOWN);
	get_print(ROTATE_LEFT);
	get_print(ROTATE_RIGHT);
	get_print(ROTATE_ON);
	get_print(ROTATE_AROUND_MOUSE_ON);
	get_print(ZOOM_TO_TEMPLE);
	get_print(ZOOM_TO_CREATURE);
	get_print(ZOOM_TO_REALM);
	get_print(ZOOM_TO_INSIDE_TEMPLE);
	get_print(ZOOM_TO_CREATURE_ROOM);
	get_print(ZOOM_TO_CHALLENGE_ROOM);
	get_print(ZOOM_TO_SAVE_GAME_ROOM);
	get_print(ZOOM_TO_OPTIONS_ROOM);
	get_print(ZOOM_TO_LIBRARY);
	get_print(LEASH_UNLEASH_CREATURE);
	get_print(SHOW_VILLAGER_NAMES);
	get_print(SHOW_VILLAGER_DETAILS);
	get_print(QUICK_SAVE);
	get_print(QUICK_LOAD);
	get_print(PREVIOUS_LEASH);
	get_print(NEXT_LEASH);

#undef get_print

#define get_print(x)                                               \
	do                                                             \
	{                                                              \
		if (Get(UnbindableActionMap::x))                           \
		{                                                          \
			SPDLOG_LOGGER_DEBUG(spdlog::get("input"), "\t{}", #x); \
		}                                                          \
	} while (0)

	get_print(DOUBLE_CLICK);
	get_print(TWO_BUTTON_CLICK);

#undef get_print

	{
		const glm::ivec2 absoluteMousePosition = PointerPosition();
		const auto screenSize =
		    Locator::windowing::has_value() ? Locator::windowing::value().GetSize() : glm::ivec2(1.0f, 1.0f);
		// While the camera is turned around the hand with the middle button, or turned and zoomed with both buttons,
		// the mouse's movement turns it and the cursor stays where it was. Once it ends, the pointer is put back where
		// the cursor was held, so the hand carries on from there. Off unless the camera allows it.
		const bool turning = _cursorFreezeAllowed && (GetBindable(BindableActionMap::ROTATE_AROUND_MOUSE_ON) ||
		                                              GetUnbindable(UnbindableActionMap::TWO_BUTTON_CLICK));
		const auto cursor = _cursorFreeze.Update(turning, absoluteMousePosition);
		if (cursor.started && UsesRealInput())
		{
			// The pointer is held in the window while the mouse's movement still comes through
			SDL_SetRelativeMouseMode(SDL_TRUE);
			_realPointerHeld = true;
		}
		if (cursor.warpTo.has_value())
		{
			if (std::exchange(_realPointerHeld, false))
			{
				SDL_SetRelativeMouseMode(SDL_FALSE);
			}
			if (UsesRealInput())
			{
				_mouseMotion.Put(WarpRealPointer(*cursor.warpTo));
			}
		}
		_mousePosition = glm::clamp(cursor.cursor, glm::zero<decltype(screenSize)>(), screenSize);
	}
	_mouseMotion.NewFrame();
	_mouseWheelDelta = 0.0f;
	_bindableMapPrevious = _bindableMap;
	_bindableMap =
	    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
	                                   ~(static_cast<uint64_t>(_mouseWheelBinding[0].value_or(BindableActionMap::NONE)) |
	                                     static_cast<uint64_t>(_mouseWheelBinding[1].value_or(BindableActionMap::NONE))));

	// After the frame's starting state is kept, so that the presses show as changes
	ApplyQueuedPresses();
}

void GameActionMap::ProcessEvent(const SDL_Event& event)
{
	if (event.type == SDL_KEYDOWN)
	{
		const auto key = event.key.keysym.scancode;
		_heldKeys.set(static_cast<size_t>(key));
		// A binding with a modifier held takes the key from the bindings without one
		const auto down = ActionsForKeyDown(_bindings, key, event.key.keysym.mod);
		_bindableMap = static_cast<BindableActionMap>(
		    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(ActionsForKey(_bindings, key))) |
		    static_cast<uint64_t>(down));
		_keyDowns.push_back(down);
	}
	// The second press of a double click with the left button marks the double click, and presses none of the button's
	// actions again. The mark outlasts the press: it stays, through the button's letting go, until the camera takes it.
	// A quick row's presses pair two by two, so its fourth, sixth and later even presses are double clicks too
	else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT &&
	         double_click::IsDoubleClick(event.button.clicks))
	{
		_lastDoubleClickPress = event.button.clicks;
		_unbindableMap = static_cast<UnbindableActionMap>(static_cast<uint8_t>(_unbindableMap) |
		                                                  static_cast<uint8_t>(UnbindableActionMap::DOUBLE_CLICK));
	}
	// The middle and the right buttons' second press of a double click hold no button: they press none of the
	// button's actions, so the camera does not turn for the middle's, and the right's makes no both buttons held with
	// the left (the right's double click reaches the hand through its own buttons, not these)
	else if (event.type == SDL_MOUSEBUTTONDOWN &&
	         (event.button.button == SDL_BUTTON_MIDDLE || event.button.button == SDL_BUTTON_RIGHT) &&
	         double_click::IsDoubleClick(event.button.clicks))
	{
	}
	else if (event.type == SDL_MOUSEBUTTONDOWN)
	{
		const auto mod = static_cast<SDL_Keymod>(_windowModifiers);
		const uint8_t buttonMask = SDL_BUTTON(event.button.button);
		const uint8_t newComboMask = _currentMouseButtons | buttonMask;
		if (newComboMask != buttonMask)
		{
			if (_mouseModBindings.contains(newComboMask) && (_mouseModBindings[newComboMask].first & mod) != 0)
			{
				_bindableMap = static_cast<BindableActionMap>(
				    // Remove non-modded action
				    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseBindings[newComboMask])) |
				    // Add modded action
				    static_cast<uint64_t>(_mouseModBindings[newComboMask].second));
				// Remove non-combo keys
				for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
				{
					const uint8_t keyMask = (1 << i);
					if ((newComboMask & keyMask) != 0u && _mouseModBindings.contains(keyMask) &&
					    (_mouseModBindings[keyMask].first & mod) != 0)
					{
						_bindableMap = static_cast<BindableActionMap>(
						    static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseModBindings[keyMask].second));
					}
				}
			}
			else if (_mouseBindings.contains(newComboMask))
			{
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
				                                              static_cast<uint64_t>(_mouseBindings[newComboMask]));
				// Remove non-combo keys
				for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
				{
					const uint8_t keyMask = (1 << i);
					if ((newComboMask & keyMask) != 0u && _mouseBindings.contains(keyMask))
					{
						_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
						                                              ~static_cast<uint64_t>(_mouseBindings[keyMask]));
					}
				}
			}
		}
		else if (_mouseModBindings.contains(buttonMask) && (_mouseModBindings[buttonMask].first & mod) != 0)
		{
			_bindableMap = static_cast<BindableActionMap>(
			    // Remove non-modded action
			    (static_cast<uint64_t>(_bindableMap) & ~static_cast<uint64_t>(_mouseBindings[buttonMask])) |
			    // Add modded action
			    static_cast<uint64_t>(_mouseModBindings[buttonMask].second));
		}
		else if (_mouseBindings.contains(buttonMask))
		{
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                              static_cast<uint64_t>(_mouseBindings[buttonMask]));
		}
		_currentMouseButtons |= buttonMask;
	}
	else if (event.type == SDL_MOUSEWHEEL)
	{
		_wheelTurned = true;
		// The notches as turned, a fraction of one from a fine wheel or a touchpad
		_mouseWheelDelta += event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -event.wheel.preciseY : event.wheel.preciseY;
		if (event.wheel.y > 0)
		{
			_bindableMap =
			    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                   static_cast<uint64_t>(_mouseWheelBinding[0].value_or(BindableActionMap::NONE)));
		}
		else if (event.wheel.y < 0)
		{
			_bindableMap =
			    static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) |
			                                   static_cast<uint64_t>(_mouseWheelBinding[1].value_or(BindableActionMap::NONE)));
		}
	}
	else if (event.type == SDL_KEYUP)
	{
		const auto key = event.key.keysym.scancode;
		_heldKeys.reset(static_cast<size_t>(key));
		_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
		                                              ~static_cast<uint64_t>(ActionsForKey(_bindings, key)));
	}
	else if (event.type == SDL_MOUSEBUTTONUP)
	{
		const uint8_t buttonMask = SDL_BUTTON(event.button.button);
		if (_mouseBindings.contains(buttonMask))
		{
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
			                                              ~static_cast<uint64_t>(_mouseModBindings[buttonMask].second));
			_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
			                                              ~static_cast<uint64_t>(_mouseBindings[buttonMask]));
		}
		// Remove combo keys
		for (uint8_t i = 0; i < SDL_BUTTON_X2; ++i)
		{
			const uint8_t keyMask = (1 << i);
			const uint8_t comboMask = keyMask | buttonMask;
			if (buttonMask != comboMask && _mouseBindings.contains(comboMask))
			{
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
				                                              ~static_cast<uint64_t>(_mouseModBindings[comboMask].second));
				_bindableMap = static_cast<BindableActionMap>(static_cast<uint64_t>(_bindableMap) &
				                                              ~static_cast<uint64_t>(_mouseBindings[comboMask]));
			}
		}
		_currentMouseButtons &= ~buttonMask;
	}
	else if (event.type == SDL_MOUSEMOTION)
	{
		// Accumulate: several motion events can arrive in one frame (Frame() resets the delta).
		_mouseMotion.Moved({event.motion.xrel, event.motion.yrel});
	}
}

std::array<std::optional<glm::vec3>, 2> GameActionMap::GetHandPositions() const
{
	auto handPositions = Locator::handSystem::value().GetPlayerHandPositions();

	return {{
	    handPositions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Left)],
	    handPositions[static_cast<size_t>(ecs::systems::HandSystemInterface::Side::Right)],
	}};
}
