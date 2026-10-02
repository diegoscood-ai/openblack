/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>

#include <glm/vec3.hpp>

namespace openblack
{

/// LH3DLib Zoomer (0x30 bytes, bw1-decomp Lionhead/LH3DLib/development/Zoomer.h): moves a value to a destination in a
/// given time with a quartic that starts from the current value and speed and ends with the destination speed and no
/// acceleration (wiki: engine-math.md, "Zoomer (LH3DLib)"). Every operation is a float one in the original's order (the
/// FPU at 24 bits).
struct Zoomer
{
	float value = 0.0f;            ///< +0x00 CurrentValue
	float destination = 0.0f;      ///< +0x04
	float destinationSpeed = 0.0f; ///< +0x08
	float speed = 0.0f;            ///< +0x0C CurrentSpeed
	// +0x10 TimeM2 is only ever zeroed (0x407D95, 0x441ADC, 0x442744): not kept
	float time = 0.0f;       ///< +0x14 CurrentTime
	float duration = 0.0f;   ///< +0x18
	float startValue = 0.0f; ///< +0x1C
	float startSpeed = 0.0f; ///< +0x20
	float c2 = 0.0f;         ///< +0x24 the coefficient of t^2 / 2
	float c3 = 0.0f;         ///< +0x28 of t^3 / 6
	float c4 = 0.0f;         ///< +0x2C of t^4 / 24

	/// Zoomer::SetPosition 0x441AC0: value = destination = start value = p, everything else 0 (0x441AC6..0x441AE8)
	void SetPosition(float position);
	/// Zoomer::SetDestinationWithSpeedAndTime 0x407D60: below 0.001 s [0x8AA3B0] (or NaN) SetPosition(target)
	/// (0x407D67..0x407DA8); otherwise the start is the current value and speed, and (c4, c3, c2) = (r1, r2, 0) M^-1 with
	/// M = rows (C, B, A), (B, A, T), (A, T, 1), A = (T T) 0.5, B = (A T) 0.33333334, C = (A A) 0.16666667, inverted by
	/// LHMatrix::SetInverse 0x7FB290 (its |det| >= 1e-10 clamp makes a step of less than 0.0493 s barely move and then
	/// jump: det = -T^6 / 144), r1 = (target - start) - T start speed, r2 = target speed - start speed
	void SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds);
	/// Zoomer::Update 0x442720: t = dt + time; at t >= duration (not "<", 0x442727) the destination and its speed;
	/// otherwise speed = ((t c2 + a c3) + b c4) + start speed and value = ((((C c4) + b c3) + a c2) + t start speed) +
	/// start value, a = (t t) 0.5, b = (t a) 0.33333334, C = (a a) 0.16666667
	void Update(float seconds);
	/// Not a function of the original: time < duration
	[[nodiscard]] bool IsMoving() const { return time < duration; }
};

/// LH3DLib Zoomer3d (0x90 bytes): x +0x00, y +0x30, z +0x60
struct Zoomer3d
{
	std::array<Zoomer, 3> axis;

	/// Zoomer::SetPosition on each axis (inline, CameraModeScript::SetCameraPosition 0x461370 ...)
	void SetPosition(const glm::vec3& position);
	/// Zoomer3d::SetDestinationWithTime 0x44E760: destination speed 0 on the three axes (x calls 0x407D60, y and z are
	/// the same code inline, 0x44E77F..0x44E9E5)
	void SetDestinationWithTime(const glm::vec3& target, float seconds);
	/// Zoomer::Update 0x442720 on x, y, z (GCamera::Update 0x441FEE..0x442029)
	void Update(float seconds);
	/// Zoomer3d::GetCurrentValue 0x4605D0
	[[nodiscard]] glm::vec3 GetCurrentValue() const;
	[[nodiscard]] glm::vec3 GetDestination() const;
	[[nodiscard]] glm::vec3 GetSpeed() const;
	[[nodiscard]] glm::vec3 GetStartValue() const;
	[[nodiscard]] glm::vec3 GetStartSpeed() const;
	[[nodiscard]] glm::vec3 GetDestinationSpeed() const;
};

} // namespace openblack
