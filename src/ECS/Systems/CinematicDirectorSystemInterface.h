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
#include <span>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Gui/ScriptTexts.h"

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
	/// As the story's scripts start a new game: the picture is black from the first frame until they fade it in
	virtual void StartStory() = 0;
	/// The scripts' restart (a new game, a playground's start): the fade stops and the picture is clear, the close
	/// clipping is off and the scripts' words and clipping window go at once; a land change keeps them all
	virtual void OnScriptsRestart() = 0;
	/// FADE_FINISHED
	[[nodiscard]] virtual bool IsFadeFinished() const = 0;
	/// The fade's colour over the picture, 0xAARRGGBB, nothing for an alpha of 0
	[[nodiscard]] virtual uint32_t GetFadeColour() const = 0;
	/// The temple's fade writes its colour over the fade's every frame it runs (video/FallingSpellVideo.h)
	virtual void SetFadeColour(uint32_t argb) = 0;

	/// Slides the cinema bars in or out over `transitionSeconds` of game time, from where they are
	virtual void SetWideScreen(bool on, float transitionSeconds) = 0;
	/// The bars at once where they are going: all the way in before a full screen film (kept there while no game time
	/// passes), all the way out at the help system's reset
	virtual void SnapWideScreen() = 0;
	[[nodiscard]] virtual bool IsWideScreenOn() const = 0;
	/// WIDESCREEN_TRANSISTION_FINISHED
	[[nodiscard]] virtual bool IsWideScreenTransitionFinished() const = 0;
	/// How far in the bars are, 0 for none and 1 for a 16:9 picture
	[[nodiscard]] virtual float GetWideScreenFraction() const = 0;

	/// A script's close clipping: the camera draws things right up to itself, for close shots
	virtual void SetCloseClipping(bool close) = 0;
	[[nodiscard]] virtual bool IsCloseClipping() const = 0;

	/// GAME_DRAW_TEXT, GAME_DRAW_TEMP_TEXT: writes words across the screen in a box, its corner and size shares of the
	/// screen's width and height and its lines' height a share of the screen's height, in the colour last set, fading in
	/// over `fadeSeconds` or shown at once for none
	virtual void WriteText(std::u16string_view words, glm::vec2 corner, glm::vec2 extent, float size, float fadeSeconds) = 0;
	/// SET_DRAW_TEXT_COLOUR: the colour the texts written from now on are drawn in, black until a script sets one; it
	/// lasts from land to land and over the scripts' restart
	virtual void SetTextColour(glm::u8vec3 colour) = 0;
	/// FADE_ALL_DRAW_TEXT: fades every text on the screen out over `seconds`, or takes them away at once for none
	virtual void FadeAllTexts(float seconds) = 0;
	/// SET_CLIPPING_WINDOW: the screen goes black but for a box, its corner and size shares of the screen's width and
	/// height, the black fading in over `seconds` or at once for none
	virtual void SetClipWindow(glm::vec2 corner, glm::vec2 extent, float seconds) = 0;
	/// CLEAR_CLIPPING_WINDOW: the black fades out over `seconds`, or goes at once for none
	virtual void ClearClipWindow(float seconds) = 0;
	[[nodiscard]] virtual const gui::script_text::ClipWindow& GetClipWindow() const = 0;
	/// Moves the clipping window's black and then the texts' fades on by the game's time of a frame, taking away the
	/// texts that have faded out
	virtual void UpdateTexts(int32_t milliseconds) = 0;
	/// The texts on the screen, the newest first, in the order they are drawn
	[[nodiscard]] virtual std::span<const gui::script_text::ScreenText> GetTexts() const = 0;

	/// Moves the fade on by a game turn
	virtual void ProcessTurn() = 0;
	/// Slides the bars on by the game time of the frame, which stops while the game is paused
	virtual void Update(std::chrono::duration<float, std::milli> gameTime) = 0;
};

} // namespace openblack::ecs::systems
