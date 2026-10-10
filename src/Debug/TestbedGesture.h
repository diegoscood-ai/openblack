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
#include <span>
#include <vector>

#include <glm/vec2.hpp>

#include "Magic/Gestures/GestureTemplates.h"

/// A gesture drawn by a testbed scenario, with no hook of its own in the game. Its mouse positions are the first
/// template of the gesture drawn back across the middle of the screen, as the Gestures window's "Draw it" draws one.
/// They reach the recogniser through the stroke player the gesture test plays its strokes with, which sends every
/// position as a mouse message of its own and leaves the real mouse out meanwhile, so that no position is dropped
/// between frames. The pointer follows the stroke, so that the hand draws it, and the circle is drawn with the Action
/// button held, as a player sizes a storm or a shield held in the hand. The plan and its steps are pure and tested on
/// their own; the runner hands them to the game.
namespace openblack::testbed_gesture
{

/// The stroke player sends a mouse message every 28 ms (the message period of the gesture input, GestureInput.cpp)
inline constexpr float k_MessageSeconds = 0.028f;
/// A stroke is as wide as the Gestures window draws a gesture, in pixels
inline constexpr float k_StrokeWidth = 320.0f;
/// The Action button stays held a little after the last message, for the recogniser's last look at the stroke
inline constexpr float k_HoldAfterSeconds = 0.25f;

/// Whether a gesture is drawn with the Action button held: the circle, which sizes a storm or a shield in the hand
[[nodiscard]] bool DrawnWithAction(magic::gestures::Gesture gesture);
/// The mouse positions of a gesture drawn across the middle of a window, from its first template; none without one
[[nodiscard]] std::vector<glm::ivec2> StrokeOf(std::span<const magic::gestures::GestureData> templates,
                                               magic::gestures::Gesture gesture, glm::ivec2 windowSize);
/// The position along a stroke of so many positions that the pointer is at some seconds after the stroke player started
/// it: the last one sent, or the first before any is
[[nodiscard]] size_t PositionAt(float seconds, size_t count);

/// A gesture being drawn
struct Drawing
{
	enum class Stage : uint8_t
	{
		/// The pointer is at the stroke's start, and the Action button down if the gesture holds it; the stroke starts
		/// the next frame, once the hand has seen the press
		Pressing,
		/// The stroke player sends the positions, and the pointer follows them
		Playing,
		/// Every position is sent; the Action button stays held a little longer
		Holding,
		Done,
	};
	std::vector<glm::ivec2> pixels;
	magic::gestures::Gesture gesture {magic::gestures::k_None};
	bool holdAction {false};
	Stage stage {Stage::Pressing};
	/// Seconds into the stage
	float seconds {0.0f};
};

/// What the runner does for a drawing in a frame
struct DrawStep
{
	/// The stroke player is given the positions
	bool startStroke {false};
	/// Where the pointer goes
	std::optional<glm::ivec2> pointer;
	/// The Action button is let go
	bool releaseAction {false};
	/// The drawing is over
	bool done {false};
};

/// A drawing of a gesture along those positions; none with fewer than two
[[nodiscard]] std::optional<Drawing> StartDrawing(std::vector<glm::ivec2> pixels, magic::gestures::Gesture gesture);
/// The drawing some seconds on, by whether the stroke player is still sending its positions
DrawStep Step(Drawing& drawing, float seconds, bool strokePlaying);
/// Whether the recogniser took a gesture since its cooldown was last read: only taking a gesture starts the cooldown
/// again, which otherwise only runs down
[[nodiscard]] bool TookAGesture(float cooldownBefore, float cooldownNow);

} // namespace openblack::testbed_gesture
