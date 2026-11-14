/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

/// The keys that move the world camera. Each key the player holds adds or takes away the same step in the order the
/// game reads them, so two keys in opposite directions cancel.
namespace openblack::camera_keys
{

/// The step of a key in one second
inline constexpr float k_StepPerSecond = 400.0f;

/// The keys the player holds in this frame
struct Held
{
	bool turnLeft {false};
	bool turnRight {false};
	bool tiltUp {false};
	bool tiltDown {false};
	bool forwards {false};
	bool backwards {false};
	bool left {false};
	bool right {false};
	/// The zoom key: the forwards and backwards keys zoom, and the left and right keys turn
	bool zoom {false};
	/// The turn modifier (the turn key, or a fight watched, never with the zoom key): the forwards and backwards keys
	/// tilt, and the left and right keys turn
	bool turnModifier {false};
};

/// What the keys ask of the camera in this frame
struct Steps
{
	/// Positive turns to the right
	float turn {0.0f};
	/// Positive tilts up
	float tilt {0.0f};
	/// Positive zooms out
	float zoom {0.0f};
	/// Positive moves backwards
	float forwards {0.0f};
	/// Positive moves to the right
	float strafe {0.0f};
};

/// The steps of the keys in a frame of these seconds (the camera's whole milliseconds times 0.001), each key adding its
/// step to its part in the order the game adds them: turn right and left, tilt down and up, then forwards, backwards,
/// left and right
[[nodiscard]] Steps Step(const Held& held, float seconds);

} // namespace openblack::camera_keys
