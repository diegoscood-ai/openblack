/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MouseButtons.h"

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
	if (state.boxTookInput || *slot.pressed == event.down)
	{
		return false;
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
	}
	return box;
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
