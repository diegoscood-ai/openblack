/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SnowfallSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/Weather/Atmos.h"
#include "ECS/Weather/WeatherLand.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::systems;

namespace
{
/// The blocks whose snow is looked at, out to past where snow is drawn
constexpr float k_BlockReach = 400.0f + 160.0f;

/// The snow over each land block near the camera: the block's four 80 x 80 tiles, along x then z, each from the most
/// snow of the weather grid at its four corners (40 either side of its centre), as the original reads it
std::vector<snowfall::Tile> IslandTiles(const glm::vec3& camera)
{
	std::vector<snowfall::Tile> tiles;
	if (!Locator::terrainSystem::has_value())
	{
		return tiles;
	}
	const glm::vec2 eye {camera.x, camera.z};
	for (const auto& block : Locator::terrainSystem::value().GetBlocks())
	{
		if (!block.GetLndBlock())
		{
			continue;
		}
		const glm::vec2 middle = block.GetMapPosition() + glm::vec2(80.0f);
		if (!(k_BlockReach > std::hypot(middle.x - camera.x, middle.y - camera.z)))
		{
			continue;
		}
		for (const auto& blockTile : snowfall::BlockTiles(block.GetMapPosition()))
		{
			int32_t snowiest = 0;
			for (const auto& sample : blockTile.samples)
			{
				snowiest =
				    std::max<int32_t>(snowiest, weather::atmos::GetWeather(glm::vec3(sample.x, 0.0f, sample.y), true).snow);
			}
			if (auto tile = snowfall::TileOf(blockTile.centre, snowiest, eye))
			{
				tile->ground = weather::LandHeightAt(tile->corner.x, tile->corner.y);
				tiles.push_back(*tile);
			}
		}
	}
	return tiles;
}
} // namespace

SnowfallSystem::SnowfallSystem()
    : SnowfallSystem([](float a, float b) { return game_random::crt::Random(a, b); }, IslandTiles)
{
}

SnowfallSystem::SnowfallSystem(snowfall::Random random, TileSource tiles)
    : _random(std::move(random))
    , _tiles(std::move(tiles))
{
}

void SnowfallSystem::Scatter()
{
	snowfall::Scatter(_flakes, _random, _height);
}

void SnowfallSystem::Update(float seconds, float fallSpeed, float height)
{
	_height = height;
	if (_present && _drawn)
	{
		_drawn = false;
		snowfall::Step(_flakes, seconds, fallSpeed, height, _random);
	}
}

std::vector<snowfall::Tile> SnowfallSystem::TakeTiles(const glm::vec3& camera)
{
	if (!_present)
	{
		return {};
	}
	auto tiles = _tiles(camera);
	if (tiles.empty())
	{
		return tiles;
	}
	_drawn = true;
	return tiles;
}
