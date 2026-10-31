/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>

#include <algorithm>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "CameraDrag.h"

namespace openblack
{

/// How much faster than the game's own speed the movement keys move the camera over the land. At the default the
/// camera moves exactly as the game moves it; turning, tilting and zooming keep the game's speed whatever this is.
constexpr float k_KeyboardMoveSpeedDefault = 1.0f;
constexpr float k_KeyboardMoveSpeedMin = 0.25f;
constexpr float k_KeyboardMoveSpeedMax = 8.0f;
/// Each step of the speed up and down keys is a half of a doubling, so two steps double or halve the speed
constexpr float k_KeyboardMoveSpeedStepsPerDoubling = 2.0f;

/// Keeps a speed within the allowed range; anything that is not a number falls back to the default
[[nodiscard]] inline float ClampKeyboardMoveSpeed(float speed)
{
	if (std::isnan(speed))
	{
		return k_KeyboardMoveSpeedDefault;
	}
	return std::clamp(speed, k_KeyboardMoveSpeedMin, k_KeyboardMoveSpeedMax);
}

/// The speed a number of steps up (or down, for negative steps) from the given one. Steps land on whole halves of a
/// doubling, so stepping back always returns to exactly the default.
[[nodiscard]] inline float StepKeyboardMoveSpeed(float speed, int steps)
{
	const auto current = std::round(std::log2(ClampKeyboardMoveSpeed(speed)) * k_KeyboardMoveSpeedStepsPerDoubling);
	return ClampKeyboardMoveSpeed(std::exp2((current + static_cast<float>(steps)) / k_KeyboardMoveSpeedStepsPerDoubling));
}

/// A pair of keys that move, turn or tilt the camera opposite ways, as the game adds them up: each key held adds its own
/// step, -1 for the first and +1 for the second, so with both held they cancel out and the camera stays still
[[nodiscard]] constexpr float KeyPairInput(bool lessHeld, bool moreHeld)
{
	return (moreHeld ? 1.0f : 0.0f) - (lessHeld ? 1.0f : 0.0f);
}

/// Whether the turn modifier is on for the movement keys: never with the zoom key held; else while a fight is watched,
/// or with the turn key held. With it on, forwards and backwards tilt and left and right turn
[[nodiscard]] constexpr bool TurnModifierOn(bool zoomHeld, bool watchingFight, bool turnHeld)
{
	return !zoomHeld && (watchingFight || turnHeld);
}

/// The helper icons by the hand that the movement keys' modifiers offer, as camera_drag::tricon hints: the zoom key held
/// offers zooming and turning, else the turn modifier (the turn key, or a fight watched) offers tilting and turning.
/// They come from the modifiers alone, whether or not a movement key is pressed, and none while the land is gripped
[[nodiscard]] constexpr uint32_t ModifierIconHints(bool zoomHeld, bool watchingFight, bool turnHeld, bool landGripped)
{
	if (landGripped)
	{
		return 0;
	}
	if (zoomHeld)
	{
		return camera_drag::tricon::k_Zoom | camera_drag::tricon::k_Rotate;
	}
	if (TurnModifierOn(zoomHeld, watchingFight, turnHeld))
	{
		return camera_drag::tricon::k_Pitch | camera_drag::tricon::k_Rotate;
	}
	return 0;
}

/// How far the movement keys move the camera this frame, given how far the game's own speed moves it
[[nodiscard]] inline float ScaleKeyboardMove(float distance, float speed)
{
	return distance * ClampKeyboardMoveSpeed(speed);
}

/// The movement keys' move over the land in a frame, as the game works it out. The keys' input (x backwards and
/// forwards, y across) is scaled by a thousandth of the zoom's step, the same step the zoom is scaled by, so the higher
/// the camera stands above its focus the further it goes. The forwards part is divided by the tangent of the pitch, kept
/// between 0.2 and 2, and the move is turned by the heading. The sine is kept unrounded and the cosine rounded to a
/// float, as the game has them.
[[nodiscard]] inline glm::vec3 KeyboardMoveOffset(glm::vec2 input, float step, float pitch, float heading)
{
	constexpr float k_StepShare = 0.001f;
	constexpr double k_LeastSlope = 0.2f;
	constexpr double k_MostSlope = 2.0;
	const float scale = step * k_StepShare;
	double slope = std::tan(static_cast<double>(pitch));
	// Not above the least (or not a number) goes to it; then not below the most goes to it
	if (!(slope > k_LeastSlope))
	{
		slope = k_LeastSlope;
	}
	else if (!(slope < k_MostSlope))
	{
		slope = k_MostSlope;
	}
	const float across = -(scale * input.y);
	const auto ahead = static_cast<float>(static_cast<double>(scale * input.x) / slope);
	const double turn = -static_cast<double>(heading);
	const auto cosine = static_cast<float>(std::cos(turn));
	const double sine = std::sin(turn);
	const float x = (across * cosine) + static_cast<float>(static_cast<double>(ahead) * -sine);
	const float z = static_cast<float>(static_cast<double>(across) * sine) + (ahead * cosine);
	return {x, 0.0f, z};
}

} // namespace openblack
