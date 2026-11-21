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

#include <SDL_video.h>

namespace openblack::windowing
{

/// What a window event does to the screen's activation
enum class Activation : std::uint8_t
{
	Unchanged,
	Deactivate,
	Reactivate
};

/// Minimising the window deactivates the screen and restoring it reactivates it. Losing or gaining the focus (switching
/// to another window without minimising this one) changes nothing, and neither does maximising
[[nodiscard]] constexpr Activation ActivationFor(std::uint8_t windowEvent)
{
	switch (windowEvent)
	{
	case SDL_WINDOWEVENT_MINIMIZED:
		return Activation::Deactivate;
	case SDL_WINDOWEVENT_RESTORED:
		return Activation::Reactivate;
	default:
		return Activation::Unchanged;
	}
}

/// Whether the screen is away (minimised) after an activation change
[[nodiscard]] constexpr bool AwayAfter(bool away, Activation activation)
{
	switch (activation)
	{
	case Activation::Deactivate:
		return true;
	case Activation::Reactivate:
		return false;
	case Activation::Unchanged:
	default:
		return away;
	}
}

/// While the screen is away the game loop goes no further than the window's events: no turn, no frame and no draw, until
/// the window is restored or the game quits. The game's timer is not stopped meanwhile
[[nodiscard]] constexpr bool WaitsForWindow(bool away, bool running)
{
	return away && running;
}

} // namespace openblack::windowing
