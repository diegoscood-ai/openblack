/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SeaCells.h"

#include <cmath>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/TerrainMaterial.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;

namespace
{
const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}
} // namespace

namespace openblack::ecs::sea_cells
{

glm::ivec2 CellOf(glm::vec3 point)
{
	return map_coords::CellOf(point);
}

glm::ivec2 RoundedCellOf(glm::vec3 point)
{
	// x * 0.1 as a float, then rounded to the nearest (the default FPU mode)
	return {static_cast<int32_t>(std::lrint(point.x * 0.1f)), static_cast<int32_t>(std::lrint(point.z * 0.1f))};
}

const lnd::LNDCell* CellAt(const LandIslandInterface& island, glm::ivec2 cell)
{
	const int32_t cells = island.GetCellsPerSide();
	if (cell.x < 0 || cell.y < 0 || cell.x >= cells || cell.y >= cells)
	{
		return nullptr;
	}
	const auto coordinates = glm::u16vec2(cell);
	if (!island.HasBlockAt(coordinates))
	{
		return nullptr;
	}
	return &island.GetCell(coordinates);
}

uint16_t AltitudeAt(const LandIslandInterface& island, glm::ivec2 cell)
{
	const auto* c = CellAt(island, cell);
	return c != nullptr ? island.GetCellAltitude(*c) : uint16_t {0};
}

bool InBounds(const LandIslandInterface& island, glm::ivec2 cell)
{
	return map_coords::InBounds(cell, island.GetCellsPerSide());
}

bool IsWater(const LandIslandInterface& island, glm::ivec2 cell)
{
	const auto* c = CellAt(island, cell);
	return c == nullptr || c->properties.hasWater != 0;
}

bool IsLand(const LandIslandInterface& island, glm::ivec2 cell)
{
	const auto* c = CellAt(island, cell);
	return c != nullptr && c->properties.hasWater == 0;
}

bool IsDryLand(const LandIslandInterface& island, glm::ivec2 cell)
{
	return AltitudeAt(island, cell) >= 4;
}

bool IsCoastal(const LandIslandInterface& island, glm::ivec2 cell)
{
	const auto* c = CellAt(island, cell);
	return c != nullptr && c->properties.hasWater == 0 && c->properties.coastLine != 0;
}

uint32_t CollideLandscape(const LandIslandInterface& island, glm::ivec2 cell)
{
	// the map cell is inside the game map (the same test as InBounds)
	if (!InBounds(island, cell))
	{
		return k_CollideEdge;
	}
	return IsWater(island, cell) ? k_CollideWater : k_CollideLand;
}

int32_t GetSurfaceType(const LandIslandInterface& island, glm::vec3 point)
{
	const auto cell = CellOf(point);
	const auto* c = CellAt(island, cell);
	if (c == nullptr)
	{
		return 6;
	}
	if (c->properties.hasWater != 0)
	{
		return 7; // !IsLand
	}
	if (!Locator::infoConstants::has_value())
	{
		return 3;
	}
	// the cell's terrain material, snow under deep lying snow (terrain_material::AtCell); a material past the table's
	// last, 43, reads its first row
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	auto row = terrain_material::AtCell(&island, glm::u16vec2(cell));
	if (row > 43)
	{
		row = 0;
	}
	const auto surface = row < info.size() ? static_cast<int32_t>(info[row].surfaceSound) : 3;
	return surface >= 1 && surface <= 8 ? surface : 3; // <= 0 or >= 9 -> 3
}

float ScriptLandHeight(const LandIslandInterface& island, glm::vec3 point)
{
	// x 0.1, then truncated, not the MapCoords high word
	const glm::ivec2 cell(static_cast<int32_t>(point.x * 0.1f), static_cast<int32_t>(point.z * 0.1f));
	if (cell.x < 0 || cell.x > 511 || cell.y < 0 || cell.y > 511 || CellAt(island, cell) == nullptr ||
	    AltitudeAt(island, cell) == 0)
	{
		return -10.0f;
	}
	return island.GetHeightAt(glm::vec2(point.x, point.z));
}

bool InBounds(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr && InBounds(*island, CellOf(point));
}

bool IsWater(glm::vec3 point)
{
	const auto* island = Island();
	return island == nullptr || IsWater(*island, CellOf(point));
}

bool IsLand(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr && IsLand(*island, CellOf(point));
}

bool IsDryLand(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr && IsDryLand(*island, CellOf(point));
}

bool IsCoastal(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr && IsCoastal(*island, CellOf(point));
}

uint32_t CollideLandscape(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr ? CollideLandscape(*island, CellOf(point)) : k_CollideEdge;
}

int32_t GetSurfaceType(glm::vec3 point)
{
	const auto* island = Island();
	return island != nullptr ? GetSurfaceType(*island, point) : 6;
}

} // namespace openblack::ecs::sea_cells
