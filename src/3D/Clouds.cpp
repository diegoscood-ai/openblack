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
#include <cstdlib>
#include <ctime>

#include "Camera/Camera.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Weather/Atmos.h"
#include "EngineConfig.h"
#include "Graphics/Lh3dColour.h"
#include "Locator.h"

namespace openblack
{
namespace
{
constexpr int k_CloudCount = 70;     // [0xBF33A8]
constexpr float k_TrackHalf = 8000.0f;
constexpr float k_Speed = 70.0f;     // units per second along the track
constexpr float k_WindCos = -0.70710678f; // cos(3 pi / 4), [0x92B2A0] = 2.35619455575943
constexpr float k_WindSin = 0.70710678f;

uint32_t s_landscapeGeneration = 0;

/// The MSVC CRT rand() 0x7C8837 that Random 0x81D180 uses (not the game's synced GRand): s = s * 214013 + 2531011,
/// (s >> 16) & 0x7FFF. The game seeds it once with srand(time(NULL)) (fn_005776E0 0x577721), so the sky is different
/// in every session; OPENBLACK_CLOUD_SEED=<n> fixes the seed (tests, screenshots). Other CRT rand() users of the
/// original share the stream, which openblack cannot reproduce; the stream here only serves the clouds and goes on
/// from land to land.
class CrtRandom
{
public:
	CrtRandom()
	{
		const char* seed = std::getenv("OPENBLACK_CLOUD_SEED");
		_state = seed != nullptr ? static_cast<uint32_t>(std::strtoul(seed, nullptr, 10))
		                         : static_cast<uint32_t>(std::time(nullptr));
	}
	int Rand() noexcept
	{
		_state = _state * 214013u + 2531011u;
		return static_cast<int>((_state >> 16) & 0x7FFFu);
	}
	/// Random 0x81D180: min + (max - min) * (rand() * 3.0518509e-05f), the float 1/32767, so max can come out
	float Random(float min, float max) noexcept
	{
		return min + (max - min) * (static_cast<float>(Rand()) * 3.0518509e-05f);
	}

private:
	uint32_t _state;
};

CrtRandom& Crt()
{
	static CrtRandom random;
	return random;
}

/// fn_005E1DE0's lerp of two D3DCOLORs: every byte a + floor((b - a) * f / 256), modulo 256
uint32_t LerpColour(uint32_t a, uint32_t b, int f) noexcept
{
	uint32_t result = 0;
	for (const uint32_t shift : {24u, 16u, 8u, 0u})
	{
		const int ca = static_cast<int>((a >> shift) & 0xFFu);
		const int cb = static_cast<int>((b >> shift) & 0xFFu);
		const int c = ca + static_cast<int>(std::floor(static_cast<float>((cb - ca) * f) / 256.0f));
		result |= (static_cast<uint32_t>(c) & 0xFFu) << shift;
	}
	return result;
}
} // namespace

void SkyAlignment::Update(float target, float milliseconds) noexcept
{
	// GLandAlignement::DrawSky 0x5E2160: step = g_game_time_inc * 0.01 * 0.1 in X (the same size in these units)
	const float step = milliseconds * 0.01f * 0.1f;
	if (target < _value)
	{
		_value = std::max(target, _value - step);
	}
	else if (target > _value)
	{
		_value = std::min(target, _value + step);
	}
}

Clouds::Clouds()
{
	// CloudInSky::Open 0x5E2439..0x5E24F4: 5 Random calls per cloud, in this order, clouds 0 and 1 too: x in
	// [-8000, 8000], y in [300, 500], z in [-5000, 5000], size +0x88 in [13, 50], k +0x8C in [2.5, 5]. Each cloud on its
	// own, uniform in the box: the sky's clouds are not placed in groups (the grouped ones are the storms' puffs,
	// GWeather::DrawClouds 0x83FC90)
	auto& random = Crt();
	_clouds.reserve(k_CloudCount);
	for (int i = 0; i < k_CloudCount; ++i)
	{
		Cloud cloud {};
		cloud.local.x = random.Random(-k_TrackHalf, k_TrackHalf);
		cloud.local.y = random.Random(300.0f, 500.0f);
		cloud.local.z = random.Random(-5000.0f, 5000.0f);
		cloud.size = random.Random(13.0f, 50.0f);
		cloud.k = random.Random(2.5f, 5.0f);
		cloud.pinned = false;
		_clouds.push_back(cloud);
	}
	// clouds 0 and 1: huge domes pinned at both ends of the track (fn_005E25C0 writes them again after every move)
	_clouds[0].local = {k_TrackHalf, 500.0f, 0.0f};
	_clouds[0].size = 300.0f;
	_clouds[0].k = 20.0f;
	_clouds[0].pinned = true;
	_clouds[1].local = {-k_TrackHalf, 500.0f, 0.0f};
	_clouds[1].size = 300.0f;
	_clouds[1].k = 20.0f;
	_clouds[1].pinned = true;
}

void Clouds::OnLandscapeOpened() noexcept
{
	++s_landscapeGeneration;
}

uint32_t Clouds::GetLandscapeGeneration() noexcept
{
	return s_landscapeGeneration;
}

float Clouds::InfluentialPlayerAlignment() noexcept
{
	// test hook: OPENBLACK_TEST_SKY_ALIGNMENT=<-1..1>, then the debug slider when moved off 0
	static const char* k_Test = std::getenv("OPENBLACK_TEST_SKY_ALIGNMENT");
	if (k_Test != nullptr)
	{
		return std::clamp(std::strtof(k_Test, nullptr), -1.0f, 1.0f);
	}
	if (Locator::config::has_value() && Locator::config::value().skyAlignment != 0.0f)
	{
		return std::clamp(Locator::config::value().skyAlignment, -1.0f, 1.0f);
	}
	// fn_0064AC30's x = (v + 1) / 2 back to v (ECS/Effects/Alignment: the most influential player at the camera)
	return ecs::effects::alignment::GetInterfaceAlignment() * 2.0f - 1.0f;
}

float Clouds::WeatherOvercastAtCamera() noexcept
{
	// [0xFA2754] = [0xD1A26C] (GCamera::Update 0x4426BA..0x4426E5, copied by GLandAlignement::DrawSky 0x5E2215):
	// (float)(int8)LH3DAtmos::GetWeatherSmooth(camera position, 1).byte3 (movsx +0x83) * 0.01 (0x8C5840), not clamped
	if (!Locator::camera::has_value())
	{
		return 0.0f;
	}
	const auto weather = openblack::weather::atmos::GetWeatherSmooth(Locator::camera::value().GetOrigin(), true);
	return static_cast<float>(weather.overcast) * 0.01f;
}

uint32_t Clouds::Colour(float alignment, uint32_t table255) noexcept
{
	static constexpr uint32_t k_Table[3] = {0x00FFFFFFu, 0xC8FFFFFFu, 0xFFAAA066u}; // 0xBF339C
	const float x = std::clamp(1.0f - alignment, 0.0f, 2.0f);
	const int i = static_cast<int>(x);
	const int f = static_cast<int>((x - static_cast<float>(i)) * 256.0f);
	const uint32_t lerped = LerpColour(k_Table[i], k_Table[std::min(i + 1, 2)], f);
	// 0x5E1F05..0x5E1F24: the lerped colour times the light table's last entry, (c l) >> 8, its alpha (byte +0x1B) kept
	const uint32_t lit = lh3d_colour::MulShr8_3KeepA(lerped, table255);
	uint32_t result = lit & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		int c = static_cast<int>((lit >> shift) & 0xFFu);
		// 0x5E1F28..0x5E1FB4: c + ((35 << 8) - 70 c) >> 8, a floor
		c = c + static_cast<int>(std::floor(static_cast<float>(8960 - 70 * c) / 256.0f));
		result |= (static_cast<uint32_t>(c) & 0xFFu) << shift;
	}
	return result;
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
			// fn_005E25C0: t = x + 8000; x = t - ftol(t / 16000) * 16000 - 8000 (only x: the same line and height)
			const float t = cloud.local.x + k_TrackHalf;
			cloud.local.x = t - static_cast<float>(static_cast<int>(t * 6.25e-5f)) * 16000.0f - k_TrackHalf;
		}
	}
}

void Clouds::AdvanceAnimation(size_t index, float milliseconds)
{
	// fn_007FA300: counter += ftol(g_game_time_inc * 0.255), the modulo only once it passes 900. The original truncates
	// every frame and loses the fraction; it is kept here so the animation does not slow down at openblack's uncapped
	// frame rates (as the map mists, RendererMists.cpp)
	// (frame_anim::MistAdvance)
	auto& cloud = _clouds[index];
	graphics::frame_anim::MistClock clock {cloud.counter, cloud.counterRemainder};
	graphics::frame_anim::MistAdvance(clock, milliseconds);
	cloud.counter = clock.counter;
	cloud.counterRemainder = clock.remainder;
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

int Clouds::EdgeAlpha(const Cloud& cloud)
{
	if (cloud.pinned)
	{
		return 192;
	}
	// fistp: rounded to the nearest
	if (cloud.local.x < -6000.0f)
	{
		return static_cast<int>(std::lrint((cloud.local.x + k_TrackHalf) * 0.1275f));
	}
	if (cloud.local.x > 6000.0f)
	{
		return static_cast<int>(std::lrint((k_TrackHalf - cloud.local.x) * 0.1275f));
	}
	return 255;
}

} // namespace openblack
