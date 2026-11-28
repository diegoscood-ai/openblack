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
/// What a mouse button is to the hand: the button of the Move or of the Action binding, or the middle button's own
enum class Role : uint8_t
{
	None,
	Move,
	Middle,
	Action,
};

struct BindingSlot
{
	bool* held;
	bool* pressed;
	/// Unset for the middle button's own, which no key presses
	bool* byKey;
};

/// The Move takes its button first, then the Action, as the game asks them in that order; the middle button bound to
/// neither is its own
Role RoleOf(const MouseButtonsState& state, MouseButton button)
{
	if (button == MouseButton::Other)
	{
		return Role::None;
	}
	if (state.bindings.move == button)
	{
		return Role::Move;
	}
	if (state.bindings.action == button)
	{
		return Role::Action;
	}
	return button == MouseButton::Middle ? Role::Middle : Role::None;
}

BindingSlot SlotOf(MouseButtonsState& state, Role role)
{
	switch (role)
	{
	case Role::Move:
		return {.held = &state.left, .pressed = &state.leftPressed, .byKey = &state.leftByKey};
	case Role::Middle:
		return {.held = &state.middle, .pressed = &state.middlePressed, .byKey = nullptr};
	case Role::Action:
		return {.held = &state.right, .pressed = &state.rightPressed, .byKey = &state.rightByKey};
	case Role::None:
		break;
	}
	return {.held = nullptr, .pressed = nullptr, .byKey = nullptr};
}

/// Whether a debug window holds the button whose down it took: kept by the button itself, whatever it is bound to
bool* InWindowOf(MouseButtonsState& state, MouseButton button)
{
	switch (button)
	{
	case MouseButton::Left:
		return &state.leftInWindow;
	case MouseButton::Middle:
		return &state.middleInWindow;
	case MouseButton::Right:
		return &state.rightInWindow;
	case MouseButton::Other:
		break;
	}
	return nullptr;
}
} // namespace

bool openblack::input::ApplyMouseButton(MouseButtonsState& state, const MouseButtonEvent& event)
{
	auto* inWindow = InWindowOf(state, event.button);
	if (inWindow == nullptr)
	{
		return false;
	}
	// the up of a click a debug window took ends its hold there, wherever it comes
	if (!event.down)
	{
		*inWindow = false;
	}
	const auto role = RoleOf(state, event.button);
	const auto slot = SlotOf(state, role);
	if (slot.held == nullptr || state.boxTookInput)
	{
		return false;
	}
	// the window's double click is its own event, latched until the frame takes it
	if (event.down && event.doubleClick && role == Role::Action)
	{
		state.actionDoubleClick = true;
	}
	// The window gives the grab button's second down of a double click as the double click alone, which only the
	// camera reads: it is no press for the hand, and its up no release
	if (event.down && event.doubleClick && role == Role::Move)
	{
		return false;
	}
	// The window gives the middle button's second down of a double click as nothing at all: it holds no button, and
	// its up sends no release
	if (event.down && event.doubleClick && role == Role::Middle)
	{
		return false;
	}
	// The window gives the Action button's second down of a double click as the double click alone. Reading it presses
	// the binding and holds the action whether or not they were already, so its up sends the release as a plain
	// press's does. Sent while the action is already held, it is a press all the same
	if (event.down && event.doubleClick && role == Role::Action)
	{
		state.actionPressedWhileHeld = state.actionPressedWhileHeld || *slot.held;
		*slot.pressed = true;
		*slot.held = true;
		return true;
	}
	if (*slot.pressed == event.down)
	{
		return false;
	}
	// a binding its key pressed is let go by the key alone, not by its button's up
	if (!event.down && slot.byKey != nullptr && *slot.byKey)
	{
		return false;
	}
	*slot.pressed = event.down;
	*slot.held = event.down;
	if (event.down && slot.byKey != nullptr)
	{
		*slot.byKey = false;
	}
	if (event.down && role == Role::Middle)
	{
		state.middlePressPosition = event.position;
	}
	return true;
}

BindingKeySent openblack::input::ApplyBindingKey(MouseButtonsState& state, HandBinding binding, bool keyHeld)
{
	if (state.boxTookInput)
	{
		return BindingKeySent::Nothing;
	}
	const auto slot = SlotOf(state, binding == HandBinding::Move ? Role::Move : Role::Action);
	if (keyHeld && !*slot.pressed)
	{
		*slot.pressed = true;
		*slot.held = true;
		*slot.byKey = true;
		return BindingKeySent::Press;
	}
	if (!keyHeld && *slot.pressed && *slot.byKey)
	{
		*slot.pressed = false;
		*slot.held = false;
		return BindingKeySent::Release;
	}
	return BindingKeySent::Nothing;
}

void openblack::input::DebugWindowTookButton(MouseButtonsState& state, const MouseButtonEvent& event)
{
	state.windowTookButton = true;
	if (auto* inWindow = InWindowOf(state, event.button); inWindow != nullptr)
	{
		*inWindow = event.down;
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

void openblack::input::ForgetPresses(MouseButtonsState& state)
{
	state.actionDoubleClick = false;
	state.actionPressedWhileHeld = false;
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
