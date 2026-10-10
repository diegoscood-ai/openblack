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

/// The scripts' fade of the picture to a colour and back (SET_FADE / SET_FADE_IN): the alpha moves linearly once per
/// game turn, and the colour is drawn over everything at the end of the frame
class ScriptFade
{
public:
	/// SET_FADE: fade from transparent to the colour in `seconds` (instant if <= 0)
	void FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds);
	/// SET_FADE_IN: from opaque back to transparent in `seconds` (instant if <= 0)
	void FadeBackToNormal(float seconds);
	/// FADE_FINISHED: no fade in progress
	[[nodiscard]] bool IsFinished() const { return _rate == 0.0f; }
	/// Once per game turn, from the script's turn
	void ProcessTurn();
	/// The fade colour as ARGB: alpha 0 means nothing is drawn
	[[nodiscard]] uint32_t GetColour() const { return _colour; }
	/// Set the colour directly, as the temple writes it every frame it runs (the falling spell's white fade,
	/// video/FallingSpellVideo.h); the rate and the current alpha of ProcessTurn stay
	void SetColour(uint32_t argb) { _colour = argb; }

private:
	float _rate {0.0f};    ///< alpha per turn, 0 when idle
	float _current {0.0f}; ///< alpha 0..255
	uint32_t _colour {0};  ///< ARGB
};

} // namespace openblack::gui
