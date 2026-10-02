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

namespace openblack
{

/// The original's full-screen fade and cinema bars.
///
/// Fade (GScript, g_game+0x250090 +0xB0..+0xB8): SET_FADE / SET_FADE_IN move the alpha linearly once per game turn
/// (ProcessFade 0x6EB9D0); the colour is drawn over everything at the end of the frame (fn_0086FEE0).
/// Cinema bars (HelpSystem +0x45E8..+0x45F0, SET_WIDESCREEN): a fraction 0..1 that slides linearly in
/// HelpSystemInfo.wideScreenTime seconds of game time (fn_005C6BB0); bars of (H - 0.5625 W) f / 2 pixels (fn_0081E8B0).
class ScreenFade
{
public:
	/// SetupScreenFadeTo 0x6EBA90 (SET_FADE): fade from transparent to the colour in `seconds` (instant if <= 0)
	void FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds);
	/// SetupScreenFadeBackToNormal 0x6EBB00 (SET_FADE_IN): from opaque back to transparent in `seconds` (instant if <= 0)
	void FadeBackToNormal(float seconds);
	/// FADE_FINISHED (fn_006EBB50): no fade in progress
	[[nodiscard]] bool IsFinished() const { return _rate == 0.0f; }
	/// ProcessFade(false) 0x6EB9D0, once per game turn (GScript::Process)
	void ProcessTurn();
	/// [0xFA51D8] as ARGB: alpha 0 means nothing is drawn
	[[nodiscard]] uint32_t GetColour() const { return _colour; }

	/// HelpSystem::SetWideScreen 0x5C6AD0 (SET_WIDESCREEN)
	void SetWideScreen(bool on, float transitionSeconds);
	/// fn_005C6BB0, every frame with the game-time milliseconds of this frame (0 while the game is paused)
	void UpdateWideScreen(float gameMilliseconds);
	/// fn_005C6C40 (PlayFullScreenMovie 0x54D9EF, after SetWideScreen(1, 0) 0x54D9E4): +0x45F0 = -FLT_MAX, so
	/// GetWideScreenPercentage 0x5C6B60 is 1 at once (with the bars on) and stays there while 0 ms are added
	void SnapWideScreen();
	/// [0xEB9950], 0 (no bars) .. 1 (the picture is 16:9)
	[[nodiscard]] float GetWideScreenFraction() const { return _wideFraction; }
	/// HelpSystem::GetWideScreenControl 0x4282F0 (+0x45E8 == 1): the bars are on or coming
	[[nodiscard]] bool IsWideScreenOn() const { return _wideOn; }
	/// WIDESCREEN_TRANSISTION_FINISHED 0x6FAC20
	[[nodiscard]] bool IsWideScreenTransitionFinished() const { return _wideOn ? _wideFraction >= 1.0f : _wideFraction <= 0.0f; }
	/// fn_0081E8B0: height of each bar in pixels
	[[nodiscard]] static int LetterboxHeight(int width, int height, float fraction);

private:
	/// GetWideScreenPercentage 0x5C6B60: |+0x45F0 * 0.001 / wideScreenTime|, 1 - that with the bars off, in [0, 1]
	[[nodiscard]] float WideScreenPercentage() const;

	float _rate {0.0f};      ///< +0xB0: alpha per turn, 0 when idle
	float _current {0.0f};   ///< +0xB4: alpha 0..255
	uint32_t _colour {0};    ///< +0xB8: ARGB
	bool _wideOn {false};    ///< +0x45E8
	float _wideTimer {0.0f}; ///< +0x45F0: milliseconds
	float _wideTime {2.0f};  ///< HelpSystemInfo.wideScreenTime
	float _wideFraction {0.0f};
};

} // namespace openblack
