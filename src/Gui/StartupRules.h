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

#include "Common/Zoomer.h"

/// The rules of what the game shows as it starts: the two logo pictures and the first run's pre-intro film. Free of
/// any game state so that the start-up screens and the tests share them.
namespace openblack::startup
{

/// The logo film holds two still pictures, each shown in turn
inline constexpr int32_t k_LogoStills = 2;

/// One still of the logo film: it fades in over 0.7 s, holds for 1.7 s and fades out over 1.1 s, the last part of it
/// still opaque. A key or a mouse button while it fades in or holds starts the fade out at once; during the fade out it
/// ends the still. Its moves are the shared Zoomer's
class LogoStill
{
public:
	LogoStill() noexcept;

	/// A frame `milliseconds` after the last, with the player pressing something or not. Returns the alpha to draw the
	/// still with this frame, 0 to 255
	uint8_t Frame(int32_t milliseconds, bool pressed) noexcept;
	/// The still has been shown for the last time
	[[nodiscard]] bool Ended() const noexcept { return _phase >= 3; }

private:
	/// Fading in, holding, fading out
	void NextPhase() noexcept;

	Zoomer _zoomer;
	int32_t _phase {0};
};

/// The alpha a logo still is drawn with for the eased position: 255 per unit, rounded down, kept within 0 and 255
[[nodiscard]] uint8_t LogoAlpha(float position) noexcept;

/// Whether the pre-intro film plays: asked for on the command line, or on a first run, before any player profile
[[nodiscard]] constexpr bool PlaysPreIntro(bool requested, bool hasProfiles) noexcept
{
	return requested || !hasProfiles;
}

/// The pre-intro shows each of its frames once, and stops after its last or on a key
[[nodiscard]] constexpr bool PreIntroGoesOn(int32_t framesShown, int32_t frameCount, bool keyPressed) noexcept
{
	return framesShown < frameCount && !keyPressed;
}

/// How long the tips screen takes to fade in when it first shows, from a frame's time in milliseconds
[[nodiscard]] constexpr float NextTipFade(float fade, int32_t milliseconds) noexcept
{
	return fade + static_cast<float>(milliseconds) * 0.001f;
}

} // namespace openblack::startup
