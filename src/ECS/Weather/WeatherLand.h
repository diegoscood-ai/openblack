/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cmath>
#include <cstdint>
#include <cstring>

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/RandomNumberManager.h"
#include "Locator.h"

// The land queries of the weather code (LH3DIsland / MapCoords of the original).

namespace openblack::weather
{
/// LH3DIsland::GetAltitude 0x803090 (0 without a land or outside it)
[[nodiscard]] inline float LandHeightAt(float x, float z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 0.0f;
	}
	try
	{
		return Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z));
	}
	catch (...)
	{
		return 0.0f; // no island
	}
}

/// MapCoords::IsWater 0x6035B0 compared with 1, as FindWhereToCreateStorm does: the cell's water bit is 0x10, so only
/// a point outside the 512 x 512 cells or without a land block (the function returns 1 there) counts
[[nodiscard]] inline bool IsWaterReturnsOne(float x, float z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto& island = Locator::terrainSystem::value();
	// MapCoords: the high word of the 16.16 coordinate x 6553.6 is the 10 m cell
	const auto cx = static_cast<int32_t>(std::floor(x / 10.0f));
	const auto cz = static_cast<int32_t>(std::floor(z / 10.0f));
	const int32_t last = island.GetCellsPerSide() - 1;
	if (cx < 0 || cx > last || cz < 0 || cz > last)
	{
		return true;
	}
	const auto& cell = island.GetCell(glm::u16vec2(cx, cz));
	lnd::LNDCell empty {}; // LandIsland's answer where there is no block
	empty.properties.fullWater = true;
	return std::memcmp(&cell, &empty, sizeof(empty)) == 0;
}

/// GRand::GameFloatRand 0x6DE530: 0 for 0, else a float in [0, x) (GData::FloatRand, the game's seeded generator)
[[nodiscard]] inline float GameFloatRand(float x)
{
	if (x == 0.0f || !Locator::rng::has_value())
	{
		return 0.0f;
	}
	const float value = Locator::rng::value().NextValue(0.0f, std::abs(x));
	return x < 0.0f ? -value : value;
}

/// GRand::GameRand 0x6DE510: 0 .. n - 1
[[nodiscard]] inline uint32_t GameRand(uint32_t n)
{
	if (n == 0 || !Locator::rng::has_value())
	{
		return 0;
	}
	return Locator::rng::value().NextValue<uint32_t>(0, n - 1);
}
} // namespace openblack::weather
