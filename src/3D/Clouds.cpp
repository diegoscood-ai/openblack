/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Clouds.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>

#include "3D/LandLight.h"
#include "Camera/Camera.h"
#include "Common/GameRandom.h"
#include "ECS/Effects/Alignment.h"
#include "ECS/Systems/CloudSystemInterface.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/SkyFrameSystemInterface.h"
#include "ECS/Systems/WeatherSystemInterface.h"
#include "EngineConfig.h"
#include "Graphics/ArgbColour.h"
#include "Locator.h"

namespace openblack
{
namespace
{
constexpr int k_CloudCount = 70;
constexpr float k_TrackHalf = 8000.0f;
constexpr float k_Speed = 70.0f;          // units per second along the track
constexpr float k_WindCos = -0.70710678f; // cos(3 pi / 4)
constexpr float k_WindSin = 0.70710678f;

/// The sky of the frame (Locator::skyFrameSystem), which keeps the count of the landscapes opened that the land
/// light reloads its cells by
openblack::ecs::systems::SkyFrameSystemInterface& SkyFrame()
{
	return openblack::Locator::skyFrameSystem::value();
}

/// The sky's clouds (Locator::cloudSystem), which a land's open lays out again
openblack::ecs::systems::CloudSystemInterface& SkyClouds()
{
	if (!openblack::Locator::cloudSystem::has_value())
	{
		std::fputs("clouds: no cloud system in the locator (Locator::cloudSystem)\n", stderr);
		std::abort();
	}
	return openblack::Locator::cloudSystem::value();
}

/// OPENBLACK_CLOUD_SEED's once-only flag, in the debug hooks' store (Locator::debugHooks)
struct CloudsDebugHooksState
{
	bool seeded {false}; ///< the seed has been read (and applied when set) at the first sky
};

CloudsDebugHooksState& CloudsDebugHooksData()
{
	if (!openblack::Locator::debugHooks::has_value())
	{
		std::fputs("clouds: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return openblack::Locator::debugHooks::value().Get<CloudsDebugHooksState>();
}

/// The clouds draw with game_random::crt::Random on the CRT rand() stream the whole game shares, not the synced
/// random stream. The original's only clock seed runs when the creature is saved (three other places seed it too),
/// so openblack makes no wall-clock seed: the stream starts at the CRT's 1 (inferred). OPENBLACK_CLOUD_SEED=<n>
/// (tests, screenshots) calls crt::Srand(n) once, right before the first land's layout: the same sky only if the CRT
/// draws before it are the same too (inferred)
void SeedCrtOnce()
{
	auto& hooks = CloudsDebugHooksData();
	if (hooks.seeded)
	{
		return;
	}
	hooks.seeded = true;
	const char* seed = std::getenv("OPENBLACK_CLOUD_SEED");
	if (seed != nullptr)
	{
		game_random::crt::Srand(static_cast<uint32_t>(std::strtoul(seed, nullptr, 10)));
	}
}

/// Lerp of two ARGB colours: every byte a + floor((b - a) * f / 256), modulo 256
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
	// step = game time step * 0.01 * 0.1 in X (the same size in these units)
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
	SeedCrtOnce();
	_clouds = Layout(game_random::crt::Random);
}

std::vector<Clouds::Cloud> Clouds::Layout(const Draw& random)
{
	// 6 draws per cloud, in this order, clouds 0 and 1 too: x in [-8000, 8000], y in [300, 500], z in [-5000, 5000],
	// the mist's start in [0, 16], size in [13, 50], k in [2.5, 5]. Each cloud on its own, uniform in the box: the
	// sky's clouds are not placed in groups (the grouped ones are the storms' puffs)
	std::vector<Cloud> clouds;
	clouds.reserve(k_CloudCount);
	for (int i = 0; i < k_CloudCount; ++i)
	{
		const auto cloudX = random(-k_TrackHalf, k_TrackHalf);
		const auto cloudY = random(300.0f, 500.0f);
		const auto cloudZ = random(-5000.0f, 5000.0f);
		// the mist is made between the position and the size, and draws its own starting counter
		const auto counter = graphics::frame_anim::MistStartCounter(random(0.0f, 16.0f));
		const auto cloudSize = random(13.0f, 50.0f);
		const auto cloudK = random(2.5f, 5.0f);
		clouds.push_back(
		    {.local = {cloudX, cloudY, cloudZ}, .size = cloudSize, .k = cloudK, .pinned = false, .counter = counter});
	}
	// clouds 0 and 1: huge domes pinned at both ends of the track (the original writes them again after every move)
	clouds[0].local = {k_TrackHalf, 500.0f, 0.0f};
	clouds[0].size = 300.0f;
	clouds[0].k = 20.0f;
	clouds[0].pinned = true;
	clouds[1].local = {-k_TrackHalf, 500.0f, 0.0f};
	clouds[1].size = 300.0f;
	clouds[1].k = 20.0f;
	clouds[1].pinned = true;
	return clouds;
}

void Clouds::OnLandscapeOpened(ecs::systems::SkyFrameSystemInterface& skyFrame, ecs::systems::CloudSystemInterface& clouds)
{
	// The land's count first, which the land light reloads its cells by, then the sky's new layout. Its draws come
	// here, when the land opens, before anything the rest of the map script makes, and not at the first drawn frame
	skyFrame.OnLandscapeOpened();
	clouds.Reset();
}

void Clouds::OnLandscapeOpened()
{
	OnLandscapeOpened(SkyFrame(), SkyClouds());
}

uint32_t Clouds::GetLandscapeGeneration() noexcept
{
	return SkyFrame().GetLandscapeGeneration();
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
	// the sky's x = (v + 1) / 2 back to v (ECS/Effects/Alignment: the most influential player at the camera)
	return ecs::effects::alignment::GetInterfaceAlignment() * 2.0f - 1.0f;
}

float Clouds::WeatherOvercastAtCamera() noexcept
{
	// the signed overcast byte of the smoothed weather at the camera position, times 0.01, not clamped
	if (!Locator::camera::has_value())
	{
		return 0.0f;
	}
	return Locator::weatherSystem::value().GetOvercast(Locator::camera::value().GetOrigin());
}

uint32_t Clouds::Colour(float alignment, uint32_t table255) noexcept
{
	static constexpr std::array<uint32_t, 3> k_Table = {0x00FFFFFFu, 0xC8FFFFFFu, 0xFFAAA066u}; // good, neutral, evil
	const float x = std::clamp(1.0f - alignment, 0.0f, 2.0f);
	const int i = static_cast<int>(x);
	const int f = static_cast<int>((x - static_cast<float>(i)) * 256.0f);
	const uint32_t lerped = LerpColour(k_Table[i], k_Table[std::min(i + 1, 2)], f);
	// the lerped colour times the light table's last entry, (c l) >> 8 per channel; its alpha is not touched
	const uint32_t lit = argb_colour::MultiplyRgbShift8KeepAlpha(lerped, table255);
	uint32_t result = lit & 0xFF000000u;
	for (const uint32_t shift : {16u, 8u, 0u})
	{
		int c = static_cast<int>((lit >> shift) & 0xFFu);
		// c + ((35 << 8) - 70 c) >> 8, a floor
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
			// t = x + 8000; x = t - trunc(t / 16000) * 16000 - 8000 (only x: the same line and height)
			const float t = cloud.local.x + k_TrackHalf;
			cloud.local.x = t - static_cast<float>(static_cast<int>(t * 6.25e-5f)) * 16000.0f - k_TrackHalf;
		}
	}
}

void Clouds::AdvanceAnimation(size_t index, float milliseconds)
{
	// counter += trunc(game time step * 0.255), the modulo only once it passes 900. The original truncates
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
	constexpr int k_Side = 40;
	if (shadowImage.size() != static_cast<size_t>(k_Side) * k_Side)
	{
		return;
	}
	for (size_t i = 0; i < _clouds.size() && i < alpha.size(); ++i)
	{
		// only with an alpha
		const auto edgeAlpha = static_cast<int>(alpha[i]);
		if (edgeAlpha == 0)
		{
			continue;
		}
		// the cloud's world x and z, y 0
		const auto position = WorldPosition(_clouds[i]);
		// a 40 x 40 stamp, not centred, alpha / 255, mode 2 (the shadow)
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
	// rounded to the nearest
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
