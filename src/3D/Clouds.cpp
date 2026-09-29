/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Clouds.h"

#include <algorithm>
#include <cmath>

#include <random>

namespace openblack
{
namespace
{
constexpr int k_CloudCount = 70;     // [0xBF33A8]
constexpr float k_TrackHalf = 8000.0f;
constexpr float k_Speed = 70.0f;     // units per second along the track
constexpr float k_WindCos = -0.70710678f; // cos(3 pi / 4)
constexpr float k_WindSin = 0.70710678f;
} // namespace

Clouds::Clouds()
{
	// CloudInSky::Open: x in [-8000, 8000], y in [300, 500], z in [-5000, 5000], size in [13, 50], k in [2.5, 5]
	std::mt19937 random(0x5E23F0);
	std::uniform_real_distribution<float> x(-k_TrackHalf, k_TrackHalf);
	std::uniform_real_distribution<float> y(300.0f, 500.0f);
	std::uniform_real_distribution<float> z(-5000.0f, 5000.0f);
	std::uniform_real_distribution<float> size(13.0f, 50.0f);
	std::uniform_real_distribution<float> k(2.5f, 5.0f);
	_clouds.reserve(k_CloudCount);
	for (int i = 0; i < k_CloudCount; ++i)
	{
		_clouds.push_back({{x(random), y(random), z(random)}, size(random), k(random), false});
	}
	// clouds 0 and 1: huge domes pinned at both ends of the track
	_clouds[0] = {{k_TrackHalf, 500.0f, 0.0f}, 300.0f, 20.0f, true};
	_clouds[1] = {{-k_TrackHalf, 500.0f, 0.0f}, 300.0f, 20.0f, true};
}

void Clouds::Update(float milliseconds)
{
	for (auto& cloud : _clouds)
	{
		if (cloud.pinned)
		{
			continue;
		}
		cloud.local.x += k_Speed * milliseconds * 0.001f;
		if (cloud.local.x > k_TrackHalf)
		{
			cloud.local.x -= 2.0f * k_TrackHalf;
		}
	}
	// the animation counter: += int(time_inc * 0.255), modulo 900
	_counterRemainder += milliseconds * 0.255f;
	const int step = static_cast<int>(_counterRemainder);
	_counterRemainder -= static_cast<float>(step);
	_counter = (_counter + step) % 900;
}

void Clouds::BuildShadowCap(const std::vector<uint8_t>& shadowImage, glm::vec2 origin, glm::u16vec2 size,
                            const std::vector<float>& alpha, std::vector<uint8_t>& cap) const
{
	constexpr int k_Side = 40;
	cap.assign(static_cast<size_t>(size.x) * size.y, 255);
	if (shadowImage.size() != static_cast<size_t>(k_Side) * k_Side)
	{
		return;
	}
	for (size_t i = 0; i < _clouds.size() && i < alpha.size(); ++i)
	{
		if (alpha[i] <= 0.0f)
		{
			continue;
		}
		const auto position = WorldPosition(_clouds[i]);
		const glm::vec2 corner = (glm::vec2(position.x, position.z) - origin) * 0.1f;
		const glm::ivec2 first(static_cast<int>(std::floor(corner.x)), static_cast<int>(std::floor(corner.y)));
		const glm::vec2 fraction = corner - glm::vec2(first);
		for (int dz = 0; dz <= k_Side; ++dz)
		{
			for (int dx = 0; dx <= k_Side; ++dx)
			{
				const int cx = first.x + dx;
				const int cz = first.y + dz;
				if (cx < 0 || cz < 0 || cx >= size.x || cz >= size.y)
				{
					continue;
				}
				// bilinear: this cell sits at (dx - fraction) texels into the image
				const float u = static_cast<float>(dx) - fraction.x;
				const float v = static_cast<float>(dz) - fraction.y;
				const auto texel = [&shadowImage](int x, int y) -> float {
					if (x < 0 || y < 0 || x >= k_Side || y >= k_Side)
					{
						return 255.0f;
					}
					return shadowImage[static_cast<size_t>(y) * k_Side + x];
				};
				const int x0 = static_cast<int>(std::floor(u));
				const int y0 = static_cast<int>(std::floor(v));
				const float fu = u - static_cast<float>(x0);
				const float fv = v - static_cast<float>(y0);
				const float s = (texel(x0, y0) * (1.0f - fu) + texel(x0 + 1, y0) * fu) * (1.0f - fv) +
				                (texel(x0, y0 + 1) * (1.0f - fu) + texel(x0 + 1, y0 + 1) * fu) * fv;
				const float limit = std::max(48.0f, 255.0f - (255.0f - s) * alpha[i] / 255.0f);
				auto& value = cap[static_cast<size_t>(cz) * size.x + cx];
				value = static_cast<uint8_t>(std::min(static_cast<float>(value), limit));
			}
		}
	}
}

glm::vec3 Clouds::WorldPosition(const Cloud& cloud)
{
	return {cloud.local.x * k_WindCos - cloud.local.z * k_WindSin + 1280.0f, cloud.local.y,
	        cloud.local.x * k_WindSin + cloud.local.z * k_WindCos + 1280.0f};
}

float Clouds::EdgeAlpha(const Cloud& cloud)
{
	if (cloud.pinned)
	{
		return 192.0f;
	}
	if (cloud.local.x < -6000.0f)
	{
		return (cloud.local.x + k_TrackHalf) * 0.1275f;
	}
	if (cloud.local.x > 6000.0f)
	{
		return (k_TrackHalf - cloud.local.x) * 0.1275f;
	}
	return 255.0f;
}

} // namespace openblack
