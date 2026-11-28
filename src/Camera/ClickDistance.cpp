/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ClickDistance.h"

#include <cmath>

#include "ScriptCamera.h"

namespace openblack::click_distance
{

float HitDistance(const glm::vec3& origin, const glm::vec3& hit)
{
	const auto d = hit - origin;
	return std::sqrt(((d.z * d.z) + (d.y * d.y)) + (d.x * d.x));
}

float MeanDistance(std::span<const float> hitDistances)
{
	float inverseSum = 1.0f / k_NoHitDistance;
	for (const float distance : hitDistances)
	{
		inverseSum = inverseSum + (1.0f / distance);
	}
	const auto count = static_cast<float>(hitDistances.size() + 1);
	return 1.0f / (inverseSum / count);
}

float TowardsFocus(float meanDistance, float focusDistance, const glm::vec3& origin, const glm::vec3& focus)
{
	float heading = 0.0f;
	float pitch = 0.0f;
	script_camera::HeadingAndPitchFromPoints(origin, focus, heading, pitch);
	const float share = pitch * k_PitchShare;
	float weight = 0.0f;
	if (0.0f < share)
	{
		weight = 1.0f <= share ? 1.0f : share;
	}
	return ((focusDistance - meanDistance) * weight) + meanDistance;
}

} // namespace openblack::click_distance
