/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <vector>

#include <glm/vec3.hpp>

#include "3D/OrientedText.h"

namespace openblack::graphics::billboard
{
struct CameraFrame;
} // namespace openblack::graphics::billboard

namespace openblack::screen_point
{
struct Lens;
} // namespace openblack::screen_point

/// The game speed's words near the top of the screen: the help system writes "paused", "slow" or "fast" there once a
/// frame, right after the dialogue texts, while the game is paused, slowed or sped up. The words fade in, and each
/// letter is a quad standing in the world, tumbling a little behind the letter before it. Pure: the caller gives the
/// clock, the speed, the camera and the screen, and draws the letters with the help text's font.
namespace openblack::help
{

/// What the indicator shows
enum class SpeedIndicatorState : int8_t
{
	None = -1, ///< normal speed and not paused: nothing
	Paused = 0,
	Slow = 1,
	Fast = 2,
};

/// The help text of the paused words (HELP_TEXT_GAME_PAUSED); those of the slow (HELP_TEXT_GAME_SLOWMO) and the fast
/// (HELP_TEXT_GAME_FASTMO) ones follow it
constexpr uint32_t k_SpeedTextPaused = 4731;
/// The help text of the words shown, which is not None
[[nodiscard]] constexpr uint32_t SpeedText(SpeedIndicatorState shown)
{
	return k_SpeedTextPaused + static_cast<uint32_t>(shown);
}

/// The fade goes up this much a millisecond of real time while the words shown are the ones to show, and down this
/// much otherwise
constexpr float k_SpeedFadeInPerMs = 0.003f;
constexpr float k_SpeedFadeOutPerMs = 0.002f;
/// The letters' tumble goes on this many radians a millisecond of real time, over a cycle of 4 pi: a turn about their
/// horizontal axis, then one about their vertical axis
constexpr float k_SpeedPhasePerMs = 0.0015f;
/// Each letter's tumble is this far behind the one before it, in radians
constexpr float k_SpeedLetterLag = 1.0f / 3.0f;
/// The letters' height, in world units, at their depth along the camera
constexpr float k_SpeedLetterSize = 2.0f;

/// Paused shows Paused whatever the speed; else a speed below 1 is Slow and one above 1 is Fast
[[nodiscard]] SpeedIndicatorState StateOf(bool paused, float speed);

/// The fade eased in and out: 0 below 0, 1 above 1, else (1 - cos(fade pi)) / 2
[[nodiscard]] float EasedFade(float fade);

/// What stops the words this frame, in the order the game asks
struct SpeedIndicatorGates
{
	/// A game is running; without one the words neither show nor reset
	bool gameRunning {true};
	/// The game is in a sequence (inside the citadel, the falling spell's film): the words reset
	bool sequenceMode {false};
	/// A script owns the game's speed: the words reset, unless the game is paused
	bool scriptOwnsSpeed {false};
	/// The developers' editor mode is on, the one that also takes the frame's input and draws the camera editor
	/// (inferred): the words reset. openblack has no such mode; whether a player can reach it is not known
	bool editorMode {false};
	/// A film is open: the words reset. The help text already draws nothing while one is open
	bool videoPlaying {false};
	/// A box with a background panel takes the input (GameInterface::IsBoxActive: the menu open or the SkipBox shown):
	/// the words reset
	bool setupBoxFlag {false};
};

/// The words' state from one frame to the next: what is shown, its fade and the letters' tumble. It starts with
/// nothing shown and is never reset: it lasts the whole game.
class SpeedIndicator
{
public:
	/// What to draw this frame
	struct Frame
	{
		SpeedIndicatorState shown;
		/// The eased fade, more than 0
		float t;
		/// The letters' depth along the camera: 15 when faint, 30 at full fade
		float depth;
		/// The tumble's phase, 0 to 4 pi
		float phase;
	};

	/// One frame of `realMs` milliseconds of real time with `state` to show. The fade goes up while the words shown are
	/// `state`, else down, and only once it is down to 0 do the shown words become `state`. A gate that resets puts
	/// the shown words to `state` at no fade. Nothing to draw when the words are None, a gate stops them, or the eased
	/// fade is 0; the tumble only goes on in frames that draw.
	[[nodiscard]] std::optional<Frame> Update(SpeedIndicatorState state, uint32_t realMs, const SpeedIndicatorGates& gates);

	[[nodiscard]] SpeedIndicatorState Shown() const noexcept { return _shown; }
	[[nodiscard]] float Fade() const noexcept { return _fade; }
	[[nodiscard]] float Phase() const noexcept { return _phase; }

private:
	SpeedIndicatorState _shown {SpeedIndicatorState::None};
	float _fade {0.0f};
	float _phase {0.0f};
};

/// The angle between the ray to the middle of the screen and the ray to the letters' row, both from the eye, at a depth
/// of 10. Each ray is made a unit long, unless it is all 0
[[nodiscard]] float Pitch(glm::vec3 centreRay, glm::vec3 topRay);

/// The screen column of a letter's middle: the word spans half the screen's width at a depth of 30, more when nearer,
/// centred across the screen
[[nodiscard]] int32_t LetterColumn(int width, size_t letters, float depth, size_t letter);

/// A letter's turn in its tumble: about its horizontal axis for the first half of the cycle, about its vertical axis for
/// the second, the angle folded into [-pi / 2, pi / 2)
struct LetterTurn
{
	bool aboutVertical {false};
	float angle {0.0f};
};
[[nodiscard]] LetterTurn TurnOf(float phase, size_t letter);

/// A letter's axes in the camera's space (x right, y up, z away from the eye): along the text, down the letters, and
/// off the page. Its turn, then tilted by the pitch, then turned upside down to face the eye
[[nodiscard]] std::array<glm::vec3, 3> LetterAxes(const LetterTurn& turn, float pitch);

/// Every letter's frame in the world, a letter a quad centred on its origin: the letters' row is a twentieth of the
/// screen's height from its top, at the frame's depth
[[nodiscard]] std::vector<openblack::TextFrame> PlaceLetters(const graphics::billboard::CameraFrame& camera,
                                                             const screen_point::Lens& lens, int width, int height,
                                                             size_t letters, float depth, float phase);

} // namespace openblack::help
