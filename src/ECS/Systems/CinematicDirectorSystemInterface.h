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

#include <chrono>

namespace openblack::ecs::systems
{

/// What the scripts do to the picture for their cut scenes: fading it to a colour and back, and the cinema bars. Made
/// with the Game and kept until it goes; the game turn steps the fade, every frame slides the bars
class CinematicDirectorSystemInterface
{
public:
	virtual ~CinematicDirectorSystemInterface() = default;

	/// SET_FADE: fades the picture from clear to a colour in `seconds`, truncated to whole seconds; at once for none
	virtual void FadeTo(uint8_t red, uint8_t green, uint8_t blue, float seconds) = 0;
	/// SET_FADE_IN: fades the picture from the colour back to clear in `seconds`, truncated; at once for none
	virtual void FadeBackToNormal(float seconds) = 0;
	/// FADE_FINISHED
	[[nodiscard]] virtual bool IsFadeFinished() const = 0;
	/// The fade's colour over the picture, 0xAARRGGBB, nothing for an alpha of 0
	[[nodiscard]] virtual uint32_t GetFadeColour() const = 0;
	/// The temple's fade writes its colour over the fade's every frame it runs (video/FallingSpellVideo.h)
	virtual void SetFadeColour(uint32_t argb) = 0;

	/// Slides the cinema bars in or out over `transitionSeconds` of game time, from where they are
	virtual void SetWideScreen(bool on, float transitionSeconds) = 0;
	/// Before a full screen film: the bars all the way in at once, and kept there while no game time passes
	virtual void SnapWideScreen() = 0;
	[[nodiscard]] virtual bool IsWideScreenOn() const = 0;
	/// WIDESCREEN_TRANSISTION_FINISHED
	[[nodiscard]] virtual bool IsWideScreenTransitionFinished() const = 0;
	/// How far in the bars are, 0 for none and 1 for a 16:9 picture
	[[nodiscard]] virtual float GetWideScreenFraction() const = 0;

	/// A script's close clipping: the camera draws things right up to itself, for close shots
	virtual void SetCloseClipping(bool close) = 0;
	[[nodiscard]] virtual bool IsCloseClipping() const = 0;

	/// Moves the fade on by a game turn
	virtual void ProcessTurn() = 0;
	/// Slides the bars on by the game time of the frame, which stops while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
};

} // namespace openblack::ecs::systems
