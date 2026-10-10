/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/Systems/GestureEventsInterface.h"

namespace openblack::ecs::systems
{

/// The gestures the player draws with the hand. Once a frame the mouse is sampled into the hand's path and the
/// interface looks for the gestures it waits for: the circle that sizes a storm or a shield, the power-up gestures of a
/// seed in the hand, the scribble that cancels, the spiral that opens the miracle selection and its stages, and the R
/// that repeats the last miracle. A recognised gesture is acted on at once.
class GestureSystemInterface: public GestureEventsInterface
{
public:
	/// What a frame tells the gestures. The cursor, the window and the camera are read from the game's services
	struct Frame
	{
		/// The frame's game time, 0 while paused
		float seconds {0.0f};
	};

	/// Once a frame: a test stroke's next points, the mouse sampling, then the interface's look for the gestures
	virtual void Update(const Frame& frame) = 0;
	/// Forgets the hand's path
	virtual void ForgetPath() = 0;

	/// The screen's width over its height, which the templates' aspect tests are measured with
	[[nodiscard]] virtual float GetScreenAspect() const = 0;
	/// How much longer the circle drawn for the storm or shield in the hand is remembered, in seconds; 0 without one
	[[nodiscard]] virtual float GetCircleSecondsLeft() const = 0;
	/// Whether the game expects a gesture of the hand, so that the gesture trail behind it shows
	[[nodiscard]] virtual bool IsGesturing() const = 0;
};

} // namespace openblack::ecs::systems
