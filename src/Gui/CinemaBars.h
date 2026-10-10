/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::gui
{

/// The cinema bars (the help system's SET_WIDESCREEN): a fraction 0..1 that slides linearly in
/// HelpSystemInfo.wideScreenTime seconds of game time; bars of (H - 0.5625 W) f / 2 pixels
class CinemaBars
{
public:
	/// SET_WIDESCREEN
	void Set(bool on, float transitionSeconds);
	/// Every frame with the game-time milliseconds of this frame (0 while the game is paused)
	void Update(float gameMilliseconds);
	/// Before a full-screen movie (after Set(1, 0)): the timer = -FLT_MAX, so the bars' percentage is 1 at once (with
	/// the bars on) and stays there while 0 ms are added
	void Snap();
	/// 0 (no bars) .. 1 (the picture is 16:9)
	[[nodiscard]] float GetFraction() const { return _fraction; }
	/// The bars are on or coming
	[[nodiscard]] bool IsOn() const { return _on; }
	/// WIDESCREEN_TRANSISTION_FINISHED
	[[nodiscard]] bool IsTransitionFinished() const { return _on ? _fraction >= 1.0f : _fraction <= 0.0f; }
	/// Height of each bar in pixels
	[[nodiscard]] static int BarHeight(int width, int height, float fraction);

private:
	/// |timer * 0.001 / wideScreenTime|, 1 - that with the bars off, in [0, 1]
	[[nodiscard]] float Fraction() const;

	bool _on {false};
	float _timer {0.0f};             ///< milliseconds
	float _transitionSeconds {2.0f}; ///< HelpSystemInfo.wideScreenTime
	float _fraction {0.0f};
};

} // namespace openblack::gui
