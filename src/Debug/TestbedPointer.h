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

#include <optional>
#include <string>

#include <glm/vec2.hpp>

#include "Input/GameCursor.h"
#include "Input/MouseButtons.h"

/// The player's mouse as a testbed scenario drives it, with no hook of its own in the game. The pointer goes where the
/// game's fixed cursor puts it (input::OverrideMouseAt, a fraction of the window), and the buttons go straight into
/// what the hand reads (input::HandButtonsRef). The maths of turning pixels into the fixed cursor's fractions and back,
/// and of moving the pointer along a sweep, is pure and tested on its own; the driver is a thin writer over it.
namespace openblack::testbed_pointer
{

/// The fraction of the window the fixed cursor is given for a pixel: the pixel's middle, so that the window's size
/// times the fraction, cut down to whole pixels as the hand's ray does, lands back on it
[[nodiscard]] glm::vec2 FractionOf(glm::ivec2 pixel, glm::ivec2 windowSize);
/// The pixel the hand's ray takes for a fraction of the window: the window's size times it, cut down to whole pixels
[[nodiscard]] glm::ivec2 PixelOf(glm::vec2 fraction, glm::ivec2 windowSize);
/// The fixed cursor's text for a fraction, "x,y", written with enough digits to read back as the same numbers
[[nodiscard]] std::string MouseAtText(glm::vec2 fraction);
/// The fraction in a fixed cursor's text, read as the game reads it; none unless it holds two numbers
[[nodiscard]] std::optional<glm::vec2> ReadMouseAt(const char* text);

/// A point given as a share of the window from its top left, at the nearest pixel
[[nodiscard]] glm::ivec2 PixelAtShare(glm::vec2 share, glm::ivec2 windowSize);
/// The pixel kept inside the window
[[nodiscard]] glm::ivec2 ClampToWindow(glm::ivec2 pixel, glm::ivec2 windowSize);

/// The button a scenario names by its number: 1 the left, 2 the middle and 3 the right, as the mouse numbers them
[[nodiscard]] input::MouseButton ButtonOf(size_t number);
/// The hand's buttons for the mouse's: it grips with the left or middle button held, and acts with the right
[[nodiscard]] input::HandButtons HandButtonsOf(const input::MouseButtonsState& buttons);

/// The pointer moving by a share of the window over some seconds, whole pixels at a time
struct Sweep
{
	glm::vec2 pixelsPerSecond {0.0f};
	float secondsLeft {0.0f};
	/// The part of a pixel left over from the steps before, carried into the next
	glm::vec2 remainder {0.0f};
};
/// A sweep by a share of the window, over some seconds; over no time at all it moves the whole way at its first step
[[nodiscard]] Sweep StartSweep(glm::vec2 share, float seconds, glm::ivec2 windowSize);
/// One step of a sweep
struct SweepStep
{
	/// The whole pixels moved by this step
	glm::ivec2 moved {0};
	/// The sweep has reached its end
	bool done {false};
};
/// The sweep some seconds on, never beyond its end
SweepStep Advance(Sweep& sweep, float seconds);

/// The fixed cursor as it was before the scenario took the pointer, kept to put back after: its text, or none
[[nodiscard]] std::optional<std::string> SaveMouseAt(const char* current);
/// What the fixed cursor is set to, to put back what was saved: the same text, or nothing, which unsets it, when there
/// was none
[[nodiscard]] std::string RestoredMouseAt(const std::optional<std::string>& saved);

/// The thin writer over the game's input: puts the pointer and the hand's buttons where a scenario says. It needs the
/// game's input state. While a hand demo plays, the demo writes the cursor and the buttons too, so a scenario that
/// drives the pointer must not run alongside one.
class Driver
{
public:
	/// Takes the pointer where the game's cursor is now, keeping the fixed cursor to put back; nothing when it is taken
	/// already
	void Take();
	[[nodiscard]] bool IsTaken() const { return _taken; }
	/// The pointer goes to a pixel, through the fixed cursor
	void MoveTo(glm::ivec2 pixel, glm::ivec2 windowSize);
	/// A button goes down or up, as the mouse's own events change the buttons
	void Press(input::MouseButton button, bool down);
	/// The buttons go to the hand, once a frame between the game reading the mouse's buttons and the hand reading them
	void WriteButtons() const;
	/// The pointer is the player's again: the fixed cursor is put back as it was and the hand's buttons are let go
	void Release();

	[[nodiscard]] glm::ivec2 GetPosition() const { return _position; }
	[[nodiscard]] const input::MouseButtonsState& GetButtons() const { return _buttons; }
	/// The buttons held, as the mouse's events carry them
	[[nodiscard]] uint32_t ButtonMask() const;
	[[nodiscard]] bool AnyHeld() const { return _buttons.left || _buttons.middle || _buttons.right; }

private:
	bool _taken {false};
	std::optional<std::string> _savedMouseAt;
	glm::ivec2 _position {0};
	input::MouseButtonsState _buttons;
};

} // namespace openblack::testbed_pointer
