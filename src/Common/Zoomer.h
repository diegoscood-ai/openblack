/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack
{

/// LH3DLib Zoomer (SetDestinationWithSpeedAndTime 0x407D60 / Update 0x442720): moves a value to a destination in a
/// given time with a quartic that starts from the current value and speed and ends with the destination speed and no
/// acceleration. Used by the hand distance (g_HandDistZoomer) and the pile sink offset (PileResource::SetSize).
struct Zoomer
{
	float value = 0.0f;
	float destination = 0.0f;
	float destinationSpeed = 0.0f;
	float speed = 0.0f;
	float time = 0.0f;
	float duration = 0.0f;
	float startValue = 0.0f;
	float startSpeed = 0.0f;
	float c2 = 0.0f; // coefficients of t^2/2, t^3/6, t^4/24
	float c3 = 0.0f;
	float c4 = 0.0f;

	void SetPosition(float position);
	void SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds);
	void Update(float seconds);
	[[nodiscard]] bool IsMoving() const { return time < duration; }
};

} // namespace openblack
