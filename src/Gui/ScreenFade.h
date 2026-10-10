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

namespace openblack::gui
{

/// The temple's fade: a full screen colour fade at 1.0 a second of frame time, shared by the citadel and the falling
/// spell; its colour goes to the script fade (video/FallingSpellVideo.h)
struct ScreenFade
{
	/// The target
	float target {0.0f};
	/// The current value. The original starts at 1.0f, but the logo screen (the first pass of the game loop, single
	/// player) sets current = target = 0, the colour to 0xFF000000 and done to 0, and a new game sets current =
	/// target = 0: at rest, as here
	float current {0.0f};
	/// The colour's RGB (the logo screen's 0xFF000000 is 0 once masked)
	uint32_t rgb {0};
	/// + 1 each time the fade reaches its target
	int32_t done {0};

	/// Covers the screen by an amount and fades it away, in the colour it has
	void FadeFrom(float amount);
	/// Covers the screen by an amount of a colour and fades it away
	void FadeFrom(float amount, uint32_t colourRgb);
	/// Fades a colour over the screen, then back away
	void FadeThrough(uint32_t colourRgb);
	/// How many times the fade has reached its target
	[[nodiscard]] int32_t GetTurns() const noexcept { return done; }

	/// Whether the fade updates this frame: unless target == current and the mode is not the citadel's (then the
	/// script fade runs); mode 3 runs neither
	[[nodiscard]] bool Runs(int32_t mode) const noexcept;
	/// One step of `deltaMs`: returns the script fade's colour. Float arithmetic, as the original's single precision
	uint32_t Update(uint32_t deltaMs) noexcept;
};

} // namespace openblack::gui
