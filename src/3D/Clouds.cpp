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

#include "3D/LandLight.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
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

/// The clouds draw with Random 0x81D180 (game_random::crt::Random) on the CRT rand() 0x7C8837 stream the whole game
/// shares, not the synced GRand. The original's only srand(time(NULL)) (0x577721) is in fn_005776E0, and only when
/// creature.lhp is saved; the other srand callers are 0x87AF37, 0x8861F7 and 0x88BB89. openblack makes no wall-clock
/// seed: the stream starts at the CRT's 1 (inferido). OPENBLACK_CLOUD_SEED=<n> (tests, screenshots) calls
/// crt::Srand(n) once, before the first sky: the same sky only if the CRT draws before it are the same too (inferido)
void SeedCrtOnce()
{
	static const bool k_Seeded = [] {
		const char* seed = std::getenv("OPENBLACK_CLOUD_SEED");
		if (seed != nullptr)
		{
			game_random::crt::Srand(static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)));
		}
		return true;
	}();
	static_cast<void>(k_Seeded);
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
	SeedCrtOnce();
	_clouds.reserve(k_CloudCount);
	for (int i = 0; i < k_CloudCount; ++i)
	{
		Cloud cloud {};
		cloud.local.x = game_random::crt::Random(-k_TrackHalf, k_TrackHalf);
		cloud.local.y = game_random::crt::Random(300.0f, 500.0f);
		cloud.local.z = game_random::crt::Random(-5000.0f, 5000.0f);
		cloud.size = game_random::crt::Random(13.0f, 50.0f);
		cloud.k = game_random::crt::Random(2.5f, 5.0f);
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
	// 0x5E1ECE..0x5E1F24 (R `imul` 0x5E1EE9, G 0x5E1F02, B 0x5E1F1E, each `sar 8`): the lerped colour times the light
	// table's last entry, (c l) >> 8; its alpha (byte +0x1B, written at 0x5E1ECA) is not touched
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

void Clouds::StampShadows(const std::vector<uint8_t>& shadowImage, const std::vector<float>& alpha) const
{
	constexpr int k_Side = 40; // 0x5E27FE push 0x28
	if (shadowImage.size() != static_cast<size_t>(k_Side) * k_Side)
	{
		return;
	}
	for (size_t i = 0; i < _clouds.size() && i < alpha.size(); ++i)
	{
		// 0x5E27CB: only with an alpha (edi)
		const auto edgeAlpha = static_cast<int>(alpha[i]);
		if (edgeAlpha == 0)
		{
			continue;
		}
		// 0x5E2769..0x5E27B5: the cloud's world x and z, y 0
		const auto position = WorldPosition(_clouds[i]);
		// 0x5E27DE..0x5E2800: fn_0086CFF0(pos, [0xD1A25C], 40, 0, alpha x (1 / 255) [0x900058], 2, 0)
		land_light::AddStamp(glm::vec3(position.x, 0.0f, position.z), shadowImage.data(), k_Side, false,
		                     static_cast<float>(edgeAlpha) * (1.0f / 255.0f), 2);
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
