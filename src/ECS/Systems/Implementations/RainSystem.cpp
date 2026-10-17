/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "RainSystem.h"

#include <cmath>

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherLand.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The C runtime's random numbers, not the game's synced ones
rain::Random CrtRandom()
{
	return [](float a, float b) { return game_random::crt::Random(a, b); };
}

/// A land block's side, and the offset of the second of the two points its rain is read at
constexpr float k_BlockSize = 160.0f;
constexpr float k_SecondSample = 40.0f;
/// The blocks whose rain is looked at, out to past where rain is drawn
constexpr float k_BlockReach = 400.0f + 160.0f;
/// Rain is drawn up to this far from the camera, thinning out from the nearer distance
constexpr float k_Farthest = 400.0f;
constexpr float k_Thinning = 100.0f;
/// The least rain that shows, and the rain's opacity of 100 for each percent
constexpr int32_t k_LeastRain = 5;
constexpr int32_t k_Opacity = 88;
} // namespace

RainSystem::RainSystem()
    : RainSystem(CrtRandom())
{
}

RainSystem::RainSystem(rain::Random random)
    : _random(std::move(random))
{
}

void RainSystem::Reset()
{
	for (auto& streak : _streaks)
	{
		streak = rain::Place(_random);
	}
	_fall = {};
	_drawn = false;
}

void RainSystem::Update(float seconds, const glm::vec3& camera)
{
	// The storm nearest the camera across the land, by its descriptor's position, sets how high and fast the rain falls
	std::optional<rain::Fall> nearest;
	float best = 0.0f;
	weather::storms::ForEach([&](const weather::storms::Storm& storm) {
		if (storm.deleteCounter != 0)
		{
			return;
		}
		const float dx = storm.descriptor.position.x - camera.x;
		const float dz = storm.descriptor.position.z - camera.z;
		const float d2 = dx * dx + dz * dz;
		if (!nearest || d2 < best)
		{
			best = d2;
			nearest = rain::Fall {.height = storm.descriptor.elevation, .speed = storm.descriptor.fallSpeed};
		}
	});
	_fall = rain::Follow(_fall, nearest);
	if (_drawn)
	{
		_drawn = false;
		rain::Step(_streaks, seconds, _fall.speed, _random);
	}
}

std::vector<rain::Tile> RainSystem::TakeTiles(const glm::vec3& camera)
{
	std::vector<rain::Tile> tiles;
	if (!Locator::terrainSystem::has_value())
	{
		return tiles;
	}
	for (const auto& block : Locator::terrainSystem::value().GetBlocks())
	{
		if (!block.GetLndBlock())
		{
			continue;
		}
		// The block's centre, within reach of the camera across the land; the tile's own cut further away is the one
		// that matters
		const glm::vec2 centre = block.GetMapPosition() + glm::vec2(k_BlockSize * 0.5f);
		const float blockDistance = std::hypot(centre.x - camera.x, centre.y - camera.z);
		if (!(k_BlockReach > blockDistance))
		{
			continue;
		}
		// The most rain or snow at two points: the centre, and the centre and 40 across
		int32_t wettest = std::numeric_limits<int32_t>::min();
		for (const auto& offset : {glm::vec2(0.0f, 0.0f), glm::vec2(k_SecondSample, k_SecondSample)})
		{
			const auto w = weather::atmos::GetWeather(glm::vec3(centre.x + offset.x, 0.0f, centre.y + offset.y), true);
			wettest = std::max({wettest, static_cast<int32_t>(w.rain), static_cast<int32_t>(w.snow)});
		}
		if (wettest <= k_LeastRain)
		{
			continue;
		}
		// 88 of 100 for each percent, in whole steps, at most 255; only a negative one is dropped
		int32_t alpha = std::min(wettest * k_Opacity / 100, 0xFF);
		if (alpha < 0)
		{
			continue;
		}
		int32_t streaks = static_cast<int32_t>(rain::k_Streaks);
		const float d = std::hypot(camera.x - centre.x, camera.z - centre.y);
		if (d > k_Farthest)
		{
			continue;
		}
		if (d > k_Thinning)
		{
			const float k = 1.0f - (d - k_Thinning) / (k_Farthest - k_Thinning);
			alpha = static_cast<int32_t>(static_cast<float>(alpha) * k);
			streaks = static_cast<int32_t>(static_cast<float>(streaks) * k);
		}
		if (streaks <= 0)
		{
			continue;
		}
		// The top is fainter, and fainter still further away
		const float distanceFactor = d / k_Farthest * 2.0f + 1.0f;
		tiles.push_back({
		    .centre = centre,
		    .streaks = std::min(streaks, static_cast<int32_t>(rain::k_Streaks)),
		    .alpha = alpha,
		    .alphaTop = static_cast<int32_t>(static_cast<float>(alpha) / (distanceFactor * 5.0f)),
		    .ground = weather::LandHeightAt(centre.x, centre.y),
		});
	}
	_drawn = _drawn || !tiles.empty();
	return tiles;
}
