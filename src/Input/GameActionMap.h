/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <bitset>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_mouse.h>
#include <glm/vec3.hpp>

#include "CursorFreeze.h"
#include "GameActionMapInterface.h"
#include "KeyBuffer.h"
#include "MouseMovement.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

namespace openblack::input
{
class GameActionMap final: public GameActionInterface
{

public:
	GameActionMap();
	GameActionMap(const GameActionMap&) = delete;
	GameActionMap& operator=(const GameActionMap&) = delete;

	[[nodiscard]] bool GetBindable(BindableActionMap action) const final;
	[[nodiscard]] bool GetUnbindable(UnbindableActionMap action) const final;
	[[nodiscard]] bool GetBindableChanged(BindableActionMap action) const final;
	[[nodiscard]] bool GetUnbindableChanged(UnbindableActionMap action) const final;
	[[nodiscard]] bool GetBindableRepeat(BindableActionMap action) const final;
	[[nodiscard]] bool GetUnbindableRepeat(UnbindableActionMap action) const final;
	[[nodiscard]] glm::uvec2 GetMousePosition() const final;
	[[nodiscard]] glm::ivec2 GetPointerPosition() const final;
	[[nodiscard]] glm::ivec2 GetMouseDelta() const final;
	[[nodiscard]] float GetMouseWheelDelta() const final;
	[[nodiscard]] SystemMouse GetSystemMouse() const final;
	[[nodiscard]] std::array<std::optional<glm::vec3>, 2> GetHandPositions() const final;

	[[nodiscard]] std::span<const KeyBinding> GetKeyBindings() const final;
	void SetKeyBinding(BindableActionMap action, std::optional<KeyChord> key) final;
	void SetMouseBinding(BindableActionMap action, MouseInput mouse) final;
	void ResetKeyBindings() final;
	void QueuePress(BindableActionMap action) final;
	[[nodiscard]] bool HasQueuedPresses() const final;
	[[nodiscard]] bool TakeDoubleClick() final;
	[[nodiscard]] uint8_t GetLastDoubleClickPress() const final { return _lastDoubleClickPress; }

	void WarpCursor(glm::ivec2 position) final;
	[[nodiscard]] std::optional<glm::ivec2> GetCursorWarp() const final;
	void SetBlockedActions(BindableActionMap actions) final { _blocked = actions; }
	void AllowCursorFreeze(bool allowed) final;
	[[nodiscard]] bool IsCursorFrozen() const final;
	[[nodiscard]] bool IsKeyHeld(SDL_Scancode key) const final;
	[[nodiscard]] std::span<const BindableActionMap> GetKeyDownsThisFrame() const final { return _keyDowns; }
	[[nodiscard]] bool TakeKeyDown(SDL_Scancode key, uint16_t modifiers) final { return _keyBuffer.Take(key, modifiers); }
	[[nodiscard]] bool IsMouseButtonHeld(MouseButton button) const final;

	void Frame() final;
	void ProcessEvent(const SDL_Event& event) final;
	void SeeDispatchedEvent(const SDL_Event& event) final;

private:
	/// The mouse's bindings from the table
	void ApplyMouseBindings();
	/// Lets go of the keys whose letting go went elsewhere, as to a text field of the debug windows or the menu
	void ReleaseKeysNoLongerHeld();
	/// Lets go of the mouse buttons whose letting go went elsewhere, as to the menu or the debug windows
	void ReleaseButtonsNoLongerHeld();
	/// Where the pointer is: the game's own cursor, which the dispatched motion events move
	[[nodiscard]] glm::ivec2 PointerPosition() const;
	/// The presses queued since the last frame, made through the same paths as the player's
	void ApplyQueuedPresses();

	KeyBindingTable _bindings {k_DefaultKeyBindings};
	/// The keys down, as pressed, so their letting go can be made up when it goes elsewhere
	std::bitset<SDL_NUM_SCANCODES> _heldKeys;
	/// The keys and mouse buttons down as the window's dispatched events left them, whoever took the events: a recording
	/// gives them back, which the real keyboard's and mouse's own state would not
	std::bitset<SDL_NUM_SCANCODES> _windowKeys;
	uint32_t _windowButtons = 0;
	/// Shift, Ctrl and Alt as the window's last key event had them, which a mouse button's binding with a modifier reads
	uint16_t _windowModifiers = KMOD_NONE;
	static constexpr uint16_t k_CountedModifiers = KMOD_SHIFT | KMOD_CTRL | KMOD_ALT;
	std::vector<BindableActionMap> _queuedPresses;
	/// This frame's keys down, the actions of each, in the order they came (GetKeyDownsThisFrame)
	std::vector<BindableActionMap> _keyDowns;
	/// The key downs the game took this frame, which a repeat of the last one's key and modifiers does not join
	KeyBuffer _keyBuffer;
	std::vector<SDL_Event> _queuedReleases;
	BindableActionMap _queuedHeld = BindableActionMap::NONE;
	BindableActionMap _bindableMap = BindableActionMap::NONE;
	UnbindableActionMap _unbindableMap = UnbindableActionMap::NONE;
	uint8_t _lastDoubleClickPress = 0;
	BindableActionMap _bindableMapPrevious = BindableActionMap::NONE;
	UnbindableActionMap _unbindableMapPrevious = UnbindableActionMap::NONE;
	std::unordered_map<int /*mousebutton*/, BindableActionMap> _mouseBindings;
	std::unordered_map<int /*mousebutton*/, std::pair<SDL_Keymod, BindableActionMap>> _mouseModBindings;
	uint8_t _currentMouseButtons = 0;
	std::array<std::optional<BindableActionMap>, 2> _mouseWheelBinding; // up, down
	glm::uvec2 _mousePosition;
	/// The mouse's own movement this frame, without the pointer's jumps when the game puts it somewhere
	MouseMovement _mouseMotion;
	float _mouseWheelDelta = 0.0f;
	/// The system's mouse, asked when the map is made; the test runs take their own instead (MouseOfRun)
	SystemMouse _systemMouse {QuerySystemMouse()};
	/// A turn of the wheel says the mouse has one
	bool _wheelTurned {false};
	/// Holds the cursor while the mouse turns the camera
	CursorFreeze _cursorFreeze;
	bool _cursorFreezeAllowed {false};
	/// The real pointer is held in the window while the cursor is frozen
	bool _realPointerHeld {false};
	std::optional<glm::ivec2> _cursorWarp;
	/// Nothing is blocked unless the land's scripts ask
	BindableActionMap _blocked = BindableActionMap::NONE;
};
} // namespace openblack::input
