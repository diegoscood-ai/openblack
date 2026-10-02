/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Rain.h"

#include <cmath>

#include <algorithm>
#include <limits>

#include "3D/LandBlock.h"
#include "3D/LandIslandInterface.h"
#include "Atmos.h"
#include "Common/GameRandom.h"
#include "Locator.h"
#include "Storms.h"
#include "WeatherLand.h"

using namespace openblack;
using namespace openblack::weather;
using namespace openblack::weather::rain;

namespace
{
std::array<Drop, k_Drops> g_drops;
float g_elevation = 160.0f; ///< 0xC38E10
float g_fallSpeed = 1.0f;   ///< 0xC38E14
bool g_drawn = false;       ///< 0xEDC300
/// ?Random@@YAMMM@Z 0x81D180 (fn_00833D10 / fn_00834700): ((rand() x k) x (max - min)) + min on the CRT rand, not
/// GRand (game_random::crt)
float Random(float min, float max)
{
	return game_random::crt::Random(min, max);
}

/// fn_00833D10
void Place(Drop& drop)
{
	drop.x = Random(-160.0f, 160.0f) * 0.5f;
	drop.z = Random(-160.0f, 160.0f) * 0.5f;
	drop.dx = Random(-15.0f, 15.0f);
	drop.dz = Random(-15.0f, 15.0f);
	drop.scroll = Random(0.0f, 1.0f);
	drop.speed = Random(0.1f, 0.2f);
	drop.phase = Random(0.0f, 1.0f);
}

/// fn_00833EA0
void Step(float seconds)
{
	const float step = seconds * 2.4f;
	for (auto& drop : g_drops)
	{
		drop.scroll += g_fallSpeed * drop.speed * seconds;
		if (drop.scroll > 1.0f)
		{
			drop.scroll -= static_cast<float>(static_cast<int32_t>(drop.scroll));
		}
		drop.phase += step;
		if (drop.phase > 1.0f)
		{
			drop.phase -= static_cast<float>(static_cast<int32_t>(drop.phase));
			drop.dx = Random(-15.0f, 15.0f);
			drop.dz = Random(-15.0f, 15.0f);
			drop.x = Random(-160.0f, 160.0f) * 0.5f;
			drop.z = Random(-160.0f, 160.0f) * 0.5f;
		}
	}
}
} // namespace

void rain::Reset()
{
	for (auto& drop : g_drops)
	{
		Place(drop);
	}
	g_elevation = 160.0f;
	g_fallSpeed = 1.0f;
	g_drawn = false;
}

void rain::Update(float seconds, const glm::vec3& camera)
{
	// the nearest storm (2D, from its descriptor's position)
	const storms::Storm* nearest = nullptr;
	float best = 0.0f;
	storms::ForEach([&](const storms::Storm& storm) {
		if (storm.deleteCounter != 0)
		{
			return;
		}
		const float dx = storm.descriptor.position.x - camera.x;
		const float dz = storm.descriptor.position.z - camera.z;
		const float d2 = dx * dx + dz * dz;
		if (nearest == nullptr || d2 < best)
		{
			best = d2;
			nearest = &storm;
		}
	});
	const float elevation = nearest != nullptr ? nearest->descriptor.elevation : 160.0f;
	const float fallSpeed = nearest != nullptr ? nearest->descriptor.fallSpeed : 1.0f;
	// (inside (inner + outer) / 2 of it the landscape's storm light 0xFA2768 follows it: not ported)
	g_elevation += (elevation - g_elevation) * 0.3f;
	g_fallSpeed += (fallSpeed - g_fallSpeed) * 0.3f;
	if (!(g_fallSpeed > 0.3f))
	{
		g_fallSpeed = 0.3f;
	}
	else if (!(g_fallSpeed < 5.0f))
	{
		g_fallSpeed = 5.0f;
	}
	if (!(g_elevation > 40.0f))
	{
		g_elevation = 40.0f;
	}
	else if (!(g_elevation < 640.0f))
	{
		g_elevation = 640.0f;
	}
	// (the lightning flash 0xEDD384 / 0xEDC374 decays here: not ported)
	if (g_drawn)
	{
		g_drawn = false;
		Step(seconds);
	}
}

std::vector<Tile> rain::CollectTiles(const glm::vec3& camera)
{
	std::vector<Tile> tiles;
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
		// the block's centre (origin + 80); its distance field +0x9BC must be under 400 + 160 (the 3D engine keeps it per
		// block; here it is the 2D distance from the camera, and fn_00834370's own 400 m cut below is what matters)
		const glm::vec2 centre = block.GetMapPosition() + glm::vec2(80.0f);
		const float blockDistance = std::hypot(centre.x - camera.x, centre.y - camera.z);
		if (!(400.0f + 160.0f > blockDistance))
		{
			continue;
		}
		// LH3DAtmos::Render3D: two samples (the centre and the centre + 40), the largest of their rain and snow
		int32_t wettest = std::numeric_limits<int32_t>::min();
		for (const auto& offset : {glm::vec2(0.0f, 0.0f), glm::vec2(40.0f, 40.0f)})
		{
			const auto w = atmos::GetWeather(glm::vec3(centre.x + offset.x, 0.0f, centre.y + offset.y), true);
			wettest = std::max({wettest, static_cast<int32_t>(w.rain), static_cast<int32_t>(w.snow)});
		}
		if (wettest <= 5)
		{
			continue;
		}
		// The Z-sorter's user data packs the tile (x / 80, z / 80) and this alpha in its three low bytes (fn_008341B0),
		// which is what fn_00833F80 unpacks for fn_00834370(x, z, 0, 0x80, alpha); over 0x2C it also spawns the water
		// drops on the ground (g_water_drop_cb, not ported)
		// Render3D 0x836431..0x83645D: 88 x max / 100 (imul 0x51EB851F); fn_008341B0 clamps it to 0xFF (0x8341B8) and
		// drops only a negative one (0x8341CC)
		int32_t alpha = std::min(wettest * 88 / 100, 0xFF);
		if (alpha < 0)
		{
			continue;
		}
		int32_t drops = k_Drops;
		const float d = std::hypot(camera.x - centre.x, camera.z - centre.y);
		if (d > 400.0f)
		{
			continue;
		}
		if (d > 100.0f)
		{
			const float k = 1.0f - (d - 100.0f) / (400.0f - 100.0f);
			alpha = static_cast<int32_t>(static_cast<float>(alpha) * k);
			drops = static_cast<int32_t>(static_cast<float>(drops) * k);
		}
		if (drops <= 0)
		{
			continue;
		}
		const float distanceFactor = d / 400.0f * 2.0f + 1.0f;
		Tile tile;
		tile.origin = glm::vec3(centre.x, LandHeightAt(centre.x, centre.y), centre.y);
		tile.drops = std::min(drops, k_Drops);
		tile.alpha = alpha;
		tile.alphaTop = static_cast<int32_t>(static_cast<float>(alpha) / (distanceFactor * 5.0f));
		tiles.push_back(tile);
	}
	return tiles;
}

void rain::MarkDrawn()
{
	g_drawn = true;
}

const std::array<Drop, k_Drops>& rain::Drops()
{
	return g_drops;
}

float rain::Elevation()
{
	return g_elevation;
}

float rain::FallSpeed()
{
	return g_fallSpeed;
}

float rain::PhaseFade(float phase)
{
	if (phase < 0.05f)
	{
		return phase * 20.0f;
	}
	if (phase > 0.95f)
	{
		return (1.0f - phase) * 20.0f;
	}
	return 1.0f;
}
