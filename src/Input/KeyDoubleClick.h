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
#include <utility>

// The Action or the Move bound to a key has a double click of its own, timed by the game rather than given by the
// system: a press of the key that comes less than a quarter of a second after the one before it is a double click, as
// the Action button's double click is (which ties the leash) and the Move button's (the camera's flight)
namespace openblack::input::key_double_click
{

/// The longest wait, in milliseconds of the real clock, after which a second press is still a double click (less
/// than this)
inline constexpr uint32_t k_DoubleClickMs = 250;

struct State
{
	/// When the key was last pressed, or when the first pass came before any press; unset before the first pass
	std::optional<uint32_t> lastPress;
	/// A press made the double click at an earlier pass, until a pass takes it
	bool latched {false};
};

/// The interface's pass of a frame, with the real clock's milliseconds now and whether the key bound to the Action was
/// pressed since the last pass. It first takes the double click an earlier pass latched, then times this pass's press
/// from the press before it (from the first pass, before any): less than k_DoubleClickMs ms after it latches a double
/// click, which the next pass gives. Every press restarts the timing. Returns whether the double click is given now
[[nodiscard]] constexpr bool Pass(State& state, bool pressed, uint32_t nowMs)
{
	if (!state.lastPress.has_value())
	{
		state.lastPress = nowMs;
	}
	const bool given = std::exchange(state.latched, false);
	if (pressed)
	{
		// the clock wraps at 2^32 ms, and the wait is taken across the wrap
		if (static_cast<uint32_t>(nowMs - *state.lastPress) < k_DoubleClickMs)
		{
			state.latched = true;
		}
		state.lastPress = nowMs;
	}
	return given;
}

/// The same timing for a reader that takes the double click only in a later frame, after this pass (the camera, which
/// reads the double click's mark at its next update): this pass's press, less than k_DoubleClickMs ms after the press
/// before it, gives the double click at once, and nothing is left latched. Returns whether this pass's press is one
[[nodiscard]] constexpr bool Press(State& state, bool pressed, uint32_t nowMs)
{
	static_cast<void>(Pass(state, pressed, nowMs));
	return std::exchange(state.latched, false);
}

} // namespace openblack::input::key_double_click
