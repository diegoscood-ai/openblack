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
	/// The down is the second click of a double click, as the window tells it
	bool doubleClick {false};
};

/// The hand's two bindings of the Controls page: Move, the grab, and Action
enum class HandBinding : uint8_t
{
	Move,
	Action,
};

/// The mouse button each of the hand's bindings has on the Controls page, if any: by default the left button is the
/// Move and the right the Action
struct HandMouseBindings
{
	std::optional<MouseButton> move {MouseButton::Left};
	std::optional<MouseButton> action {MouseButton::Right};
};

struct MouseButtonsState
{
	/// The buttons the interface holds, which the hand reads: set by a press, cleared by a release or by a box. `left`
	/// is the Move binding's and `right` the Action binding's, whichever button or key they are bound to (the left and
	/// the right buttons by default); `middle` is the middle button's while it is bound to neither
	bool left {false};
	bool middle {false};
	bool right {false};
	/// Which buttons the Move and the Action take, as the Controls page last bound them
	HandMouseBindings bindings;
	/// The Move's and the Action's last press came from their key: their button's up then releases nothing, only the
	/// key let go does
	bool leftByKey {false};
	bool rightByKey {false};
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
	/// The Action (right) button was double clicked since the frame last took it
	bool actionDoubleClick {false};
	/// The Action button's double click pressed the action while it was already held, since the frame last took it
	bool actionPressedWhileHeld {false};
};

/// A button's down or up that reaches the game, as the binding it is the button of (`state.bindings`: the Move first,
/// then the Action, the middle button's own otherwise; a button bound to neither sends nothing). The rules below name
/// the buttons by their default bindings: the grab button is the Move's, the Action button the Action's.
/// A down sends the press unless its binding is pressed already (then it sends nothing); an up sends the release only
/// when its binding is pressed, and not by its key. The press holds the button, the release
/// lets it go; a middle press keeps its position. Nothing is sent from the events that come after a frame pass where
/// a box took the input, until the next pass, nor for any other button. The Action button's down that is the second
/// click of a double click also latches the double click, until the frame takes it.
/// The Action button's second click of a double click presses its binding and holds the action even when they were
/// already, so its up sends the release as a plain press's does; when the action was already held, that press is
/// latched until the frame takes it.
/// The grab (left) button's second click of a double click sends neither a press nor, with its up, a release: the
/// window gives it as the double click alone, which only the camera reads. The middle button's second click of a
/// double click sends nothing either: the window gives it as nothing at all.
/// Returns whether a press or a release was sent
bool ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event);

/// What a binding's key sent at the interface's pass
enum class BindingKeySent : uint8_t
{
	Nothing,
	Press,
	Release,
};

/// The key bound to the Move or the Action, read once a frame at the interface's pass, as their buttons' events are:
/// held while the binding is not pressed, it sends the press (pressed by its key), which holds it; no longer held while
/// its key pressed it, it sends the release. The Action's press while the action is still held (after its double click)
/// is latched as the button's is. Nothing while a box took the input at the last pass
BindingKeySent ApplyBindingKey(MouseButtonsState& state, HandBinding binding, bool keyHeld);

/// The Action button's double click, once a frame: whether it was double clicked since the last call, which takes it.
/// Not while the temple's interior is up, which keeps it from the hand
[[nodiscard]] bool TakeActionDoubleClick(MouseButtonsState& state, bool templeInteriorUp);

/// Whether the Action button's double click pressed the action while it was already held since the last call, which
/// takes it: the hand, which reads the action as held or not once a frame, takes it as a press
[[nodiscard]] bool TakeActionPressWhileHeld(MouseButtonsState& state);

/// A save's input reset: the double click and the press while held not yet taken are forgotten; the buttons held stay
/// held
void ForgetPresses(MouseButtonsState& state);

/// A debug window took a mouse button's down or up: it takes the input as a box does, at the next frame pass, and
/// while it holds a button whose down it took
void DebugWindowTookButton(MouseButtonsState& state, const MouseButtonEvent& event);

/// The interface's pass of every frame: a box takes the input while the escape menu or the SkipBox is up (`menuUp`),
/// or a debug window took a mouse button since the last pass or still holds one. Then the interface's buttons are
/// cleared with the Action button's double click, and the caller ends the hand's action; the bindings stay as they are.
/// Returns whether a box took it
[[nodiscard]] bool BoxTakesInput(MouseButtonsState& state, bool menuUp);

/// The hand grips while the Move (the left button by default) is held. The middle button only turns the camera: it is
/// no grip, so while it turns the camera the hand keeps its normal hover
[[nodiscard]] constexpr bool HandGripping(const MouseButtonsState& state)
{
	return state.left;
}

/// The hand's action while the Action (the right button by default) is held
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
