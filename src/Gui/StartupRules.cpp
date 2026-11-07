/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StartupRules.h"

#include <algorithm>

namespace openblack::startup
{

namespace
{
/// A frame's milliseconds go to the moves' clock in seconds
constexpr float k_SecondsPerMillisecond = 0.001f;

/// The logo still's three moves: where it goes and in how long
struct Move
{
	float destination;
	float seconds;
};
constexpr Move k_FadeIn {.destination = 1.0f, .seconds = 0.7f};
constexpr Move k_Hold {.destination = 2.0f, .seconds = 1.7f};
constexpr Move k_FadeOut {.destination = 0.0f, .seconds = 1.1f};
} // namespace

uint8_t LogoAlpha(float position) noexcept
{
	// Rounded towards zero, as the game converts
	const auto alpha = static_cast<int32_t>(position * 255.0f);
	return static_cast<uint8_t>(std::clamp(alpha, 0, 255));
}

LogoStill::LogoStill() noexcept
{
	_zoomer.SetDestinationWithSpeedAndTime(k_FadeIn.destination, 0.0f, k_FadeIn.seconds);
}

void LogoStill::NextPhase() noexcept
{
	++_phase;
	const auto& move = _phase == 1 ? k_Hold : k_FadeOut;
	_zoomer.SetDestinationWithSpeedAndTime(move.destination, 0.0f, move.seconds);
}

uint8_t LogoStill::Frame(int32_t milliseconds, bool pressed) noexcept
{
	_zoomer.Update(static_cast<float>(milliseconds) * k_SecondsPerMillisecond);
	// The frame shows the alpha from before anything pressed changes it
	const uint8_t alpha = LogoAlpha(_zoomer.GetValue());
	if (pressed && _phase < 2)
	{
		// Straight to fully opaque, and from there the fade out
		_phase = 1;
		_zoomer.SetPosition(1.0f);
		NextPhase();
	}
	else if (pressed && _phase == 2)
	{
		_zoomer.SetPosition(0.0f);
		NextPhase();
	}
	else if (_zoomer.HasArrived())
	{
		NextPhase();
	}
	return alpha;
}

} // namespace openblack::startup
