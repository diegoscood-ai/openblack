/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Zoomer.h"

#include <glm/mat4x3.hpp>

#include "3D/ObjectMatrix.h"

using namespace openblack;

namespace
{
constexpr float k_MinSeconds = 0.001f; ///< [0x8AA3B0]
constexpr float k_Half = 0.5f;         ///< [0x8AA3B4]
constexpr float k_Third = 0.33333334f; ///< [0x8AB26C] = 0x3EAAAAAB
constexpr float k_Sixth = 0.16666667f; ///< [0x8AB268] = 0x3E2AAAAB
} // namespace

void Zoomer::SetPosition(float position)
{
	value = destination = startValue = position;
	speed = destinationSpeed = startSpeed = 0.0f;
	time = duration = 0.0f;
	c2 = c3 = c4 = 0.0f;
}

void Zoomer::SetDestinationWithSpeedAndTime(float target, float targetSpeed, float seconds)
{
	// fcomp [0x8AA3B0]; test ah, 1 (0x407D67..0x407D75): C0 is set by "<" and by an unordered compare
	if (!(seconds >= k_MinSeconds))
	{
		SetPosition(target); // 0x407D77..0x407DA8, the same stores as SetPosition
		return;
	}
	startSpeed = speed; // 0x407DBC
	startValue = value; // 0x407DC9
	destination = target;
	destinationSpeed = targetSpeed;
	duration = seconds;
	time = 0.0f;
	const float a = (seconds * seconds) * k_Half; // 0x407DAB..0x407DC3
	const float b = (a * seconds) * k_Third;      // 0x407DD0..0x407DE0
	const float c = (a * a) * k_Sixth;            // 0x407DE6..0x407DF9
	// the matrix on the stack, esp+0x10..+0x3C (0x407E0A..0x407E40), translation 0
	const glm::mat4x3 m(glm::vec3(c, b, a), glm::vec3(b, a, seconds), glm::vec3(a, seconds, 1.0f), glm::vec3(0.0f));
	const auto inv = lh_matrix::Inverse(m);                              // 0x407E44
	const float r1 = (destination - startValue) - duration * startSpeed; // 0x407E49..0x407E55
	const float r2 = destinationSpeed - startSpeed;                      // 0x407E57..0x407E5D
	c4 = (inv[1][0] * r2 + inv[0][0] * r1) + inv[3][0];                  // 0x407E6D..0x407E81, 0x407EC5
	c3 = (inv[0][1] * r1 + inv[1][1] * r2) + inv[3][1];                  // 0x407E83..0x407EA5
	c2 = (inv[1][2] * r2 + inv[0][2] * r1) + inv[3][2];                  // 0x407EA1..0x407EC2
}

void Zoomer::Update(float seconds)
{
	const float t = seconds + time; // fld dt; fadd [+0x14] (0x442720..0x442724)
	time = t;
	// fcom [+0x18]; test ah, 1; jne (0x442727..0x442732): the curve while t < duration (or unordered)
	if (t >= duration)
	{
		value = destination; // 0x442734..0x44274E
		speed = destinationSpeed;
		time = duration;
		return;
	}
	const float a = (t * t) * k_Half;                                       // 0x442751..0x442757
	const float b = (t * a) * k_Third;                                      // 0x44275D..0x442761
	speed = ((t * c2 + a * c3) + b * c4) + startSpeed;                      // 0x442767..0x44277D
	const float c = (a * a) * k_Sixth;                                      // 0x442780..0x442784
	value = ((((c * c4) + b * c3) + a * c2) + t * startSpeed) + startValue; // 0x44278A..0x4427A5
}

void Zoomer3d::SetPosition(const glm::vec3& position)
{
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetPosition(position[i]);
	}
}

void Zoomer3d::SetDestinationWithTime(const glm::vec3& target, float seconds)
{
	// x: push T; push 0; push p.x; call 0x407D60 (0x44E770..0x44E77A). y and z inline (0x44E77F..0x44E9E5): the same
	// threshold and matrix, the vector times the inverse by fn_00418A50 (((v.z m2i + v.y m1i) + v.x m0i) + t.i) with
	// v.z = 0, which only adds a 0 to the sums of 0x407D60
	for (int i = 0; i < 3; ++i)
	{
		axis[i].SetDestinationWithSpeedAndTime(target[i], 0.0f, seconds);
	}
}

void Zoomer3d::Update(float seconds)
{
	for (auto& zoomer : axis)
	{
		zoomer.Update(seconds);
	}
}

glm::vec3 Zoomer3d::GetCurrentValue() const
{
	return {axis[0].value, axis[1].value, axis[2].value};
}

glm::vec3 Zoomer3d::GetDestination() const
{
	return {axis[0].destination, axis[1].destination, axis[2].destination};
}

glm::vec3 Zoomer3d::GetSpeed() const
{
	return {axis[0].speed, axis[1].speed, axis[2].speed};
}

glm::vec3 Zoomer3d::GetStartValue() const
{
	return {axis[0].startValue, axis[1].startValue, axis[2].startValue};
}

glm::vec3 Zoomer3d::GetStartSpeed() const
{
	return {axis[0].startSpeed, axis[1].startSpeed, axis[2].startSpeed};
}

glm::vec3 Zoomer3d::GetDestinationSpeed() const
{
	return {axis[0].destinationSpeed, axis[1].destinationSpeed, axis[2].destinationSpeed};
}
