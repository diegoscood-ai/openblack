/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Sun.h"

#include <algorithm>

#include <glm/geometric.hpp>

#include "3D/SkyDome.h"
#include "GameClock.h"

using namespace openblack;
using namespace openblack::graphics;

std::optional<sun::Placement> sun::Place(float scriptHour)
{
	const float t = scriptHour;
	const float height = 7500.0f * (std::clamp(std::min(t, 24.0f - t), 6.0f, 12.0f) - 6.0f) / 6.0f;
	// Fading in from 3 to 6 and out from 18 to 21
	float alpha = 255.0f;
	if (t < 3.0f || t > 21.0f)
	{
		alpha = 0.0f;
	}
	else if (t < 6.0f)
	{
		alpha = (t - 3.0f) * 85.0f;
	}
	else if (t > 18.0f)
	{
		alpha = 255.0f - (t - 18.0f) * 85.0f;
	}
	if (alpha <= 0.0f)
	{
		return std::nullopt;
	}
	return Placement {.position = {-30000.0f, height, -30000.0f}, .alpha = alpha};
}

float sun::EaseGlare(float glare, int hiddenSamples, uint32_t frameMilliseconds)
{
	const float target = (1.0f - 0.2f * static_cast<float>(hiddenSamples)) * 255.0f;
	// The factor is not capped: a frame's game time stays below 200 milliseconds
	const float factor = static_cast<float>(frameMilliseconds) * game_clock::k_FractionPerMs;
	const float eased = (target - glare) * factor;
	return std::clamp(eased + glare, 0.0f, 255.0f);
}

std::array<glm::vec3, sun::k_GlareSamples.size()> sun::GlareSamplePoints(glm::vec3 sunPosition, glm::vec3 camera,
                                                                         float nearClip)
{
	// The unit line from the camera to the sun, made the near clip distance long; none when the camera is at the sun
	const glm::vec3 toSun = sunPosition - camera;
	const float length = glm::length(toSun);
	const glm::vec3 nudge = length != 0.0f ? toSun / length * nearClip : glm::vec3(0.0f);
	std::array<glm::vec3, k_GlareSamples.size()> points {};
	std::ranges::transform(k_GlareSamples, points.begin(), [&](const glm::vec2& offset) {
		glm::vec3 point = sunPosition + glm::vec3(offset, 0.0f) + nudge;
		point.y = std::max(point.y, k_GlareLowestSample);
		return point;
	});
	return points;
}

int sun::HiddenSamples(std::span<const glm::vec3> points, const std::function<bool(const glm::vec3&)>& landInTheWay)
{
	return static_cast<int>(std::ranges::count_if(points, landInTheWay));
}

int sun::GlareAlpha(float glare, float sunAlpha, float overcast, bool fog)
{
	// Multiplied in this order, each step in float, then cut to a whole number before and after the overcast
	return static_cast<int>(sky_dome::ThroughOvercast(glare * sunAlpha * (1.0f / 255.0f), overcast, fog));
}
