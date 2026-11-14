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
#include "3D/LandLightFrame.h"
#include "Common/GameRandom.h"
#include "ECS/Systems/WaterRingSystemInterface.h"
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
	if (_present && _drawn)
	{
		_drawn = false;
		rain::Step(_streaks, seconds, _fall.speed, _random);
	}
}

std::vector<rain::Tile> RainSystem::TakeTiles(const glm::vec3& camera)
{
	std::vector<rain::Tile> tiles;
	bool queued = false;
	// without its rain the game queues no block's rain: nothing drawn, no splash, and the streaks stay still
	if (!_present || !Locator::terrainSystem::has_value())
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
		if (!rain::Queued(wettest))
		{
			continue;
		}
		// The game counts its rain as drawn once a block's rain is queued to be drawn, however far the block is and
		// whatever it then shows: that alone moves the streaks on in the next update
		queued = true;
		// 88 of 100 for each percent, in whole steps, at most 255; only a negative one is dropped
		const int32_t strength = rain::Strength(wettest);
		if (strength < 0)
		{
			continue;
		}
		// A block too far for its streaks, or with none left, still splashes when its rain is strong enough: the game
		// makes the splashes from the block's strength, whatever its streaks
		const bool splashes = strength > rain::k_LeastSplashStrength;
		int32_t alpha = strength;
		int32_t streaks = static_cast<int32_t>(rain::k_Streaks);
		const float d = std::hypot(camera.x - centre.x, camera.z - centre.y);
		if (d > k_Farthest)
		{
			streaks = 0;
		}
		else if (d > k_Thinning)
		{
			const float k = 1.0f - (d - k_Thinning) / (k_Farthest - k_Thinning);
			alpha = static_cast<int32_t>(static_cast<float>(alpha) * k);
			streaks = static_cast<int32_t>(static_cast<float>(streaks) * k);
		}
		// The game draws a block's streaks only when its strength, not its most rain, is more than the least that shows
		if (!rain::ShowsStreaks(strength))
		{
			streaks = 0;
		}
		if (streaks <= 0)
		{
			if (!splashes)
			{
				continue;
			}
			streaks = 0;
			alpha = 0;
		}
		// The top is fainter, and fainter still further away
		const float distanceFactor = d / k_Farthest * 2.0f + 1.0f;
		tiles.push_back({
		    .centre = centre,
		    .streaks = std::min(streaks, static_cast<int32_t>(rain::k_Streaks)),
		    .alpha = alpha,
		    .alphaTop = streaks > 0 ? static_cast<int32_t>(static_cast<float>(alpha) / (distanceFactor * 5.0f)) : 0,
		    .ground = weather::LandHeightAt(centre.x, centre.y),
		    .strength = strength,
		});
	}
	_drawn = _drawn || queued;
	return tiles;
}

void RainSystem::Splash(const rain::Tile& tile, uint8_t drops)
{
	const auto alpha = rain::SplashAlpha(tile.strength);
	if (!_splashes || !alpha.has_value())
	{
		return;
	}
	// The colour of the land's light of this frame, with the rain's own opacity
	const uint32_t argb = (static_cast<uint32_t>(*alpha) << 24u) | (FrameLandLight(rain::k_SplashLight) & 0xFFFFFFu);
	const rain::HeightAt heightAt = [](glm::vec2 at) { return weather::LandHeightAt(at.x, at.y); };
	for (uint8_t i = 0; i < drops; ++i)
	{
		// The numbers are drawn whether or not there is room for the ring
		const auto ring = rain::Splash(tile.centre, argb, _random, heightAt);
		if (Locator::waterRingSystem::has_value())
		{
			Locator::waterRingSystem::value().Add(ring);
		}
	}
}
