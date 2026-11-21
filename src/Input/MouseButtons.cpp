/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseButtons.h"

#include <utility>

using namespace openblack::input;

namespace
{
struct ButtonSlot
{
	bool* held;
	bool* pressed;
	bool* inWindow;
};

ButtonSlot SlotOf(MouseButtonsState& state, MouseButton button)
{
	switch (button)
	{
	case MouseButton::Left:
		return {.held = &state.left, .pressed = &state.leftPressed, .inWindow = &state.leftInWindow};
	case MouseButton::Middle:
		return {.held = &state.middle, .pressed = &state.middlePressed, .inWindow = &state.middleInWindow};
	case MouseButton::Right:
		return {.held = &state.right, .pressed = &state.rightPressed, .inWindow = &state.rightInWindow};
	case MouseButton::Other:
		break;
	}
	return {.held = nullptr, .pressed = nullptr, .inWindow = nullptr};
}
} // namespace

bool openblack::input::ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event)
{
	const auto slot = SlotOf(state, event.button);
	if (slot.held == nullptr)
	{
		return false;
	}
	// the up of a click a debug window took ends its hold there, wherever it comes
	if (!event.down)
	{
		*slot.inWindow = false;
	}
	if (state.boxTookInput)
	{
		return false;
	}
	// the window's double click is its own event, latched until the frame takes it
	if (event.down && event.doubleClick && event.button == MouseButton::Right)
	{
		state.actionDoubleClick = true;
	}
	// The window gives the grab button's second down of a double click as the double click alone, which only the
	// camera reads: it is no press for the hand, and its up no release
	if (event.down && event.doubleClick && event.button == MouseButton::Left)
	{
		return false;
	}
	// The window gives the middle button's second down of a double click as nothing at all: it holds no button, and
	// its up sends no release
	if (event.down && event.doubleClick && event.button == MouseButton::Middle)
	{
		return false;
	}
	// The window gives the Action button's second down of a double click as the double click alone, which presses and
	// holds the action as a press does but leaves the binding unpressed: its up sends no release, so the action stays
	// held until the next plain press's up
	if (event.down && event.doubleClick && event.button == MouseButton::Right)
	{
		state.actionPressedWhileHeld = state.actionPressedWhileHeld || *slot.held;
		*slot.held = true;
		return true;
	}
	if (*slot.pressed == event.down)
	{
		return false;
	}
	// a press of the Action button while the action is still held (after its double click) is a press all the same
	if (event.down && event.button == MouseButton::Right && *slot.held)
	{
		state.actionPressedWhileHeld = true;
	}
	*slot.pressed = event.down;
	*slot.held = event.down;
	if (event.down && event.button == MouseButton::Middle)
	{
		state.middlePressPosition = event.position;
	}
	return true;
}

void openblack::input::DebugWindowTookButton(MouseButtonsState& state, const MouseButtonEvent& event)
{
	state.windowTookButton = true;
	if (const auto slot = SlotOf(state, event.button); slot.inWindow != nullptr)
	{
		*slot.inWindow = event.down;
	}
}

bool openblack::input::BoxTakesInput(MouseButtonsState& state, bool menuUp)
{
	const bool box = menuUp || state.windowTookButton || state.leftInWindow || state.middleInWindow || state.rightInWindow;
	state.windowTookButton = false;
	state.boxTookInput = box;
	if (box)
	{
		state.left = false;
		state.middle = false;
		state.right = false;
		state.actionDoubleClick = false;
		state.actionPressedWhileHeld = false;
	}
	return box;
}

bool openblack::input::TakeActionDoubleClick(MouseButtonsState& state, bool templeInteriorUp)
{
	return std::exchange(state.actionDoubleClick, false) && !templeInteriorUp;
}

bool openblack::input::TakeActionPressWhileHeld(MouseButtonsState& state)
{
	return std::exchange(state.actionPressedWhileHeld, false);
}

glm::ivec2 openblack::input::MouseMotion(MouseMotionState& state, const glm::ivec2& position)
{
	if (!state.previous.has_value())
	{
		state.previous = position;
	}
	const auto delta = position - *state.previous;
	state.previous = position;
	return delta;
}
