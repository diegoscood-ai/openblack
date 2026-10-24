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
/// still opaque. A press (HeldInput::LogoPressed) while it fades in or holds starts the fade out at once; during the
/// fade out it ends the still. Its moves are the shared Zoomer's
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

/// The keyboard and the mouse as the start-up screens read them: the key last pressed, none again as soon as any key is
/// let go, and the left and right mouse buttons, each held from its press to its release. Clearing forgets both, as the
/// game does after a press it has acted on
class HeldInput
{
public:
	/// A key went down (a key held repeats this): it is the key last pressed
	void KeyDown(bool escape) noexcept { _escape = escape; }
	/// Any key was let go: no key is held any more
	void KeyUp() noexcept { _escape = false; }
	/// The left (or the right) mouse button went down or up
	void Button(bool left, bool down) noexcept
	{
		const uint8_t bit = left ? 1u : 2u;
		_buttons = static_cast<uint8_t>(down ? (_buttons | bit) : (_buttons & ~bit));
	}
	void Clear() noexcept
	{
		_escape = false;
		_buttons = 0;
	}
	/// The key last pressed is Escape, and no key has been let go since
	[[nodiscard]] bool EscapeHeld() const noexcept { return _escape; }
	/// What cuts a logo still short: Escape held, or the left or the right mouse button held
	[[nodiscard]] bool LogoPressed() const noexcept { return _escape || _buttons != 0; }

private:
	bool _escape {false};
	uint8_t _buttons {0};
};

/// The alpha a logo still is drawn with for the eased position: 255 per unit, rounded down, kept within 0 and 255
[[nodiscard]] uint8_t LogoAlpha(float position) noexcept;

/// Whether the pre-intro film plays: asked for on the command line, or on a first run, before any player profile
[[nodiscard]] constexpr bool PlaysPreIntro(bool requested, bool hasProfiles) noexcept
{
	return requested || !hasProfiles;
}

/// The pre-intro shows each of its frames once, and stops after its last or when Escape is held as a frame ends
/// (HeldInput::EscapeHeld)
[[nodiscard]] constexpr bool PreIntroGoesOn(int32_t framesShown, int32_t frameCount, bool escapeHeld) noexcept
{
	return framesShown < frameCount && !escapeHeld;
}

/// Whether the films of a game being started can't be skipped (until the first one ends): only when no player had a
/// profile as it started, the new player's first game. Every other game's films can be
[[nodiscard]] constexpr bool FilmsUnskippable(bool hasProfiles) noexcept
{
	return !hasProfiles;
}

/// How long the tips screen takes to fade in when it first shows, from a frame's time in milliseconds
[[nodiscard]] constexpr float NextTipFade(float fade, int32_t milliseconds) noexcept
{
	return fade + static_cast<float>(milliseconds) * 0.001f;
}

} // namespace openblack::startup
