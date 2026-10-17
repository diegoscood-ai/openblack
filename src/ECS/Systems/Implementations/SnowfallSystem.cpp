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

/// The snow over each land block near the camera, from the same two points of the weather grid the rain reads (its
/// centre and 40 beyond it), so that no other cell of the grid is worked out
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
		const glm::vec2 centre = block.GetMapPosition() + glm::vec2(80.0f);
		if (!(k_BlockReach > std::hypot(centre.x - camera.x, centre.y - camera.z)))
		{
			continue;
		}
		int32_t snowiest = 0;
		for (const auto& offset : {glm::vec2(0.0f, 0.0f), glm::vec2(40.0f, 40.0f)})
		{
			const auto weather = weather::atmos::GetWeather(glm::vec3(centre.x + offset.x, 0.0f, centre.y + offset.y), true);
			snowiest = std::max<int32_t>(snowiest, weather.snow);
		}
		if (auto tile = snowfall::TileOf(centre, snowiest, eye))
		{
			tile->ground = weather::LandHeightAt(tile->corner.x, tile->corner.y);
			tiles.push_back(*tile);
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
	if (_drawn)
	{
		_drawn = false;
		snowfall::Step(_flakes, seconds, fallSpeed, height, _random);
	}
}

std::vector<snowfall::Tile> SnowfallSystem::TakeTiles(const glm::vec3& camera)
{
	auto tiles = _tiles(camera);
	if (tiles.empty())
	{
		return tiles;
	}
	_drawn = true;
	return tiles;
}
