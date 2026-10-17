/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Sun.h"

#include <cmath>

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
	// The line from the camera to the sun, made the near clip distance long; none when the camera is at the sun. In the
	// original's float order: the squares summed z, y, then x, the near clip over the length, then each part times that
	const glm::vec3 toSun = sunPosition - camera;
	glm::vec3 nudge(0.0f);
	if (toSun != glm::vec3(0.0f))
	{
		const float sum = ((toSun.z * toSun.z) + (toSun.y * toSun.y)) + (toSun.x * toSun.x);
		nudge = toSun * (nearClip / std::sqrt(sum));
	}
	std::array<glm::vec3, k_GlareSamples.size()> points {};
	std::ranges::transform(k_GlareSamples, points.begin(), [&](const glm::vec2& offset) {
		glm::vec3 point = sunPosition + glm::vec3(offset, 0.0f) + nudge;
		point.y = std::max(point.y, k_GlareLowestSample);
		return point;
	});
	return points;
}

int sun::HiddenSamples(std::span<const glm::vec3> points, const std::function<bool(size_t, const glm::vec3&)>& hidden)
{
	int count = 0;
	for (size_t i = 0; i < points.size(); ++i)
	{
		count += hidden(i, points[i]) ? 1 : 0;
	}
	return count;
}

std::optional<glm::ivec2> sun::SamplePixel(const glm::vec4& clip, float nearClip, glm::vec2 resolution)
{
	// The depth from the camera is the clip's w; the screen's middle is half its size across and down
	if (clip.w < nearClip)
	{
		return std::nullopt;
	}
	const auto x = static_cast<int32_t>((clip.x / clip.w + 1.0f) * (resolution.x * 0.5f));
	const auto y = static_cast<int32_t>((1.0f - clip.y / clip.w) * (resolution.y * 0.5f));
	if (x < 0 || y < 0 || static_cast<float>(x) >= resolution.x || static_cast<float>(y) >= resolution.y)
	{
		return std::nullopt;
	}
	return glm::ivec2(x, y);
}

float sun::GlareHidingDepth(float nearClip)
{
	// 1 - near / depth below 65000 / 65535, rounded to 16 bits: below 64999.5 / 65535
	constexpr float k_Hidden = 64999.5f / 65535.0f;
	return nearClip / (1.0f - k_Hidden);
}

int sun::GlareAlpha(float glare, float sunAlpha, float overcast, bool fog)
{
	// Multiplied in this order, each step in float, then cut to a whole number before and after the overcast
	return static_cast<int>(sky_dome::ThroughOvercast(glare * sunAlpha * (1.0f / 255.0f), overcast, fog));
}
