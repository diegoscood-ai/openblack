/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FightOrbit.h"

#include <cmath>

#include <limits>

#include "3D/ObjectMatrix.h"

namespace openblack::fight_orbit
{

namespace
{
constexpr float k_Pi = 3.14159274f;
constexpr float k_TwoPi = 6.28318548f;
constexpr float k_InverseTwoPi = 0.159154937f;
/// The fighters' line has a heading only when they are further apart than this along x or z
constexpr float k_SameSpot = 0.01f;
/// A whole number of turns too big for an int comes out as the smallest int, as the original's conversion does
constexpr float k_IntRange = 2147483648.0f;

/// How far apart two points are across the land, z first as the original sums them
float AcrossTheLand(glm::vec3 from, glm::vec3 to)
{
	const float dx = from.x - to.x;
	const float dz = from.z - to.z;
	return std::sqrt(dz * dz + dx * dx);
}
} // namespace

bool WantToQuit(glm::vec3 arenaCentre, float arenaRadius, glm::vec3 camera, glm::vec3 focus, float share)
{
	const float radius = arenaRadius * share;
	const float fromCamera = AcrossTheLand(arenaCentre, camera);
	if (radius * k_QuitFocusRadii < AcrossTheLand(arenaCentre, focus) && radius * k_QuitCameraRadii < fromCamera)
	{
		return true;
	}
	return radius * k_QuitFarRadii < fromCamera;
}

float WrapAngle(float angle)
{
	if (angle >= -k_Pi && angle <= k_Pi)
	{
		return angle;
	}
	const float turns = angle * k_InverseTwoPi;
	const float whole = std::abs(turns) < k_IntRange ? static_cast<float>(static_cast<int32_t>(turns)) : -k_IntRange;
	float rest = (turns - whole) * k_TwoPi;
	for (int i = 0; i < 2; ++i)
	{
		if (rest > k_Pi)
		{
			rest -= k_TwoPi;
		}
		if (rest < -k_Pi)
		{
			rest += k_TwoPi;
		}
	}
	return rest;
}

glm::vec3 Middle(glm::vec3 fighterA, glm::vec3 fighterB)
{
	return {(fighterA.x + fighterB.x) * 0.5f, (fighterA.y + fighterB.y) * 0.5f, (fighterA.z + fighterB.z) * 0.5f};
}

float Spacing(glm::vec3 fighterA, glm::vec3 fighterB)
{
	return AcrossTheLand(fighterA, fighterB);
}

float OrbitDistance(float distance, float spacing, float radiusA, float radiusB)
{
	return (spacing + radiusB * distance) + radiusA;
}

float Zoom(float distance, float zoom, float spacing, float radiusA, float radiusB)
{
	// A second fighter with no radius divides by 0 as the original does: a zoom out comes to +infinity, which ends the
	// watch, a zoom in to -infinity and nothing to not a number, which both clamp to 0. That needs IEEE floats
	static_assert(std::numeric_limits<float>::is_iec559);
	const float zoomed = OrbitDistance(distance, spacing, radiusA, radiusB) + zoom * k_ZoomShare;
	return ((zoomed - spacing) - radiusA) / radiusB;
}

float ClampDistance(float distance)
{
	if (!(distance > 0.0f))
	{
		return 0.0f;
	}
	return distance < k_MaxDistance ? distance : k_MaxDistance;
}

float ClampPitch(float pitch)
{
	if (!(pitch > k_MinPitch))
	{
		return k_MinPitch;
	}
	return pitch < k_MaxPitch ? pitch : k_MaxPitch;
}

float FightersHeading(glm::vec3 fighterA, glm::vec3 fighterB, float current)
{
	const glm::vec3 line = fighterB - fighterA;
	if (std::abs(line.x) > k_SameSpot || std::abs(line.z) > k_SameSpot)
	{
		return static_cast<float>(affine::GetYAngle(line));
	}
	return current;
}

float TurnTowards(float current, float heading)
{
	return current + WrapAngle(WrapAngle(heading) - WrapAngle(current));
}

float EaseSeconds(float modeSeconds)
{
	const float pace = modeSeconds > k_EaseSettleSeconds
	                       ? k_EaseSettledPace
	                       : modeSeconds / k_EaseSettleSeconds * (k_EaseSettledPace - k_EaseStartPace) + k_EaseStartPace;
	return pace * k_EaseScale;
}

} // namespace openblack::fight_orbit
