/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandLandPoint.h"

#include <cmath>

#include "Common/GUtilsX87.h"

namespace openblack::ecs::hand_land_point
{

float LengthZyx(glm::vec3 v)
{
	return std::sqrt((v.z * v.z + v.y * v.y) + v.x * v.x);
}

float DistanceXzy(glm::vec3 from, glm::vec3 to)
{
	const glm::vec3 d(from.x - to.x, from.y - to.y, from.z - to.z);
	return std::sqrt((d.x * d.x + d.z * d.z) + d.y * d.y);
}

Line ShiftedAlongView(glm::vec3 eye, glm::vec3 nearPoint, float depth, float nearClip)
{
	glm::vec3 d(nearPoint.x - eye.x, nearPoint.y - eye.y, nearPoint.z - eye.z);
	// not above 0, a NaN too, takes the near clip
	const float shift = depth > 0.0f ? depth : nearClip;
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		const float inverse = 1.0f / LengthZyx(d);
		d.x = d.x * inverse;
		d.y = d.y * inverse;
		d.z = d.z * inverse;
	}
	d.x = d.x * shift;
	d.y = d.y * shift;
	d.z = d.z * shift;
	return {
	    .from = {d.x + eye.x, d.y + eye.y, d.z + eye.z},
	    .to = {d.x + nearPoint.x, d.y + nearPoint.y, d.z + nearPoint.z},
	};
}

glm::vec3 AlongMouseRay(glm::vec3 eye, glm::vec3 nearPoint, float length)
{
	glm::vec3 d(nearPoint.x - eye.x, nearPoint.y - eye.y, nearPoint.z - eye.z);
	if (d.x != 0.0f || d.y != 0.0f || d.z != 0.0f)
	{
		const float scale = length / LengthZyx(d);
		d.x = scale * d.x;
		d.y = scale * d.y;
		d.z = scale * d.z;
	}
	return {eye.x + d.x, eye.y + d.y, eye.z + d.z};
}

Result HandLandPoint(const Inputs& in, const std::function<float(glm::vec2)>& heightAt)
{
	glm::vec3 point = in.handPosition;
	if (in.hitXZ.has_value())
	{
		point = {in.hitXZ->x, heightAt(*in.hitXZ), in.hitXZ->y};
	}
	else if (in.holdingWithSpring)
	{
		point = AlongMouseRay(in.eye, in.nearPoint, in.reach);
	}
	const bool found = in.hitXZ.has_value() || in.holdingWithSpring;
	// a NaN height is not standing either
	const bool subtract = in.subtractHand && point.y >= k_LowestStandingHeight;

	// with nothing found the point is the hand's position, whose distance is never shortened
	const float length = LengthZyx({in.eye.x - point.x, in.eye.y - point.y, in.eye.z - point.z});
	const float distance = found && subtract ? length - (k_HandDepthPerScale * in.handScale) * in.handSize : length;

	// at or below 2, or a NaN, gives 2; then no further than the reach (a NaN reach keeps the distance)
	float viewDistance = in.reach;
	if (!(distance > k_NearestViewDistance))
	{
		viewDistance = k_NearestViewDistance;
	}
	else if (!(distance >= in.reach))
	{
		viewDistance = distance;
	}
	return {.point = point, .viewDistance = viewDistance, .found = found};
}

float ClampedViewDistance(float distance, float reach)
{
	if (!(distance > k_NearestViewDistance))
	{
		return k_NearestViewDistance;
	}
	return reach > distance ? distance : reach;
}

float ScaleForViewDistance(float distance, float reach)
{
	namespace x87 = gutils::x87;
	const float d = ClampedViewDistance(distance, reach);
	// The power as the game's C runtime works it out on the FPU, 0.8 being the float's value
	float scale = d < 10.0f ? x87::Store(x87::Pow(x87::Div(d, 10.0f), 0.8f)) : 1.0f;
	if (d > 150.0f)
	{
		// In the game's order: (d - 150) / (1800 - 150) x 0.3 taken from 1, times d / 150, times the scale
		const double fade = x87::Sub(1.0, x87::Mul(x87::Div(x87::Sub(d, 150.0), x87::Sub(1800.0, 150.0)), 0.3f));
		scale = x87::Store(x87::Mul(x87::Mul(fade, x87::Div(d, 150.0)), scale));
	}
	return scale;
}

} // namespace openblack::ecs::hand_land_point
