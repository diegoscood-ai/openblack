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

#include <optional>

#include <glm/vec2.hpp>

// The mouse buttons and motion as the hand reads them, kept apart from SDL so they can be tested with fake events

namespace openblack::input
{
enum class MouseButton : uint8_t
{
	Left,
	Middle,
	Right,
	Other,
};

/// One button going down or up, at a window position
struct MouseButtonEvent
{
	MouseButton button {MouseButton::Other};
	bool down {false};
	glm::ivec2 position {0, 0};
};

struct MouseButtonsState
{
	/// The buttons the interface holds, which the hand reads: set by a press, cleared by a release or by a box
	bool left {false};
	bool middle {false};
	bool right {false};
	/// Where the middle button last went down (the cursor goes back there when it is let go)
	glm::ivec2 middlePressPosition {0, 0};
	/// Each button's binding: pressed from the down that sends the press to the up that sends the release. A box does
	/// not clear it, so a button held when a box came is still pressed after it
	bool leftPressed {false};
	bool middlePressed {false};
	bool rightPressed {false};
	/// The buttons whose down a debug window took and whose up has not come yet
	bool leftInWindow {false};
	bool middleInWindow {false};
	bool rightInWindow {false};
	/// A debug window took a mouse button's down or up since the last frame pass
	bool windowTookButton {false};
	/// A box took the input at the last frame pass: the button events until the next pass are dropped
	bool boxTookInput {false};
};

/// A button's down or up that reaches the game. A down sends the press unless its binding is pressed already (then it
/// sends nothing); an up sends the release only when its binding is pressed. The press holds the button, the release
/// lets it go; a middle press keeps its position. Nothing is sent from the events that come after a frame pass where
/// a box took the input, until the next pass, nor for any other button. Returns whether a press or a release was sent
bool ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event);

/// A debug window took a mouse button's down or up: it takes the input as a box does, at the next frame pass, and
/// while it holds a button whose down it took
void DebugWindowTookButton(MouseButtonsState& state, const MouseButtonEvent& event);

/// The interface's pass of every frame: a box takes the input while the escape menu or the SkipBox is up (`menuUp`),
/// or a debug window took a mouse button since the last pass or still holds one. Then the interface's buttons are
/// cleared, and the caller ends the hand's action; the bindings stay as they are. Returns whether a box took it
[[nodiscard]] bool BoxTakesInput(MouseButtonsState& state, bool menuUp);

/// The hand grips while the left or the middle button is held
[[nodiscard]] constexpr bool HandGripping(const MouseButtonsState& state)
{
	return state.middle || state.left;
}

/// The hand's action while the right button is held
[[nodiscard]] constexpr bool HandAction(const MouseButtonsState& state)
{
	return state.right;
}

struct MouseMotionState
{
	/// Unset until the first frame, which takes its own position as the previous one
	std::optional<glm::ivec2> previous;
};

/// The cursor's motion since the previous call (0 on the first call)
[[nodiscard]] glm::ivec2 MouseMotion(MouseMotionState& state, const glm::ivec2& position);
} // namespace openblack::input
