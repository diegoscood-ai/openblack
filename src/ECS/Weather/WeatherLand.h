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

#include <glm/vec2.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Locator.h"

// The land queries of the weather code.

namespace openblack::weather
{
/// The land's altitude (0 without a land or outside it)
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

/// The original's water test compared with 1, as the climates' storm placement does: the cell's water bit is 0x10, so
/// only a point outside the 512 x 512 cells or in a cell of no land block (the test returns 1 there) counts, whatever
/// the cell holds
[[nodiscard]] inline bool IsOffMapOrNoLand(float x, float z)
{
	if (!Locator::terrainSystem::has_value())
	{
		return true;
	}
	// the high word of the 16.16 coordinate x 6553.6 is the 10 m cell (unsigned), against the map's 512 cells
	const auto at = map_coords::CellOf(glm::vec2(x, z));
	if (!map_coords::InBounds(at))
	{
		return true;
	}
	return !Locator::terrainSystem::value().HasBlockAt(glm::u16vec2(at));
}

/// The game's synced random numbers (Common/GameRandom.h)
using game_random::GameFloatRand;
using game_random::GameRand;
} // namespace openblack::weather
