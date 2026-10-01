/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SeaCells.h"

#include <algorithm>
#include <cmath>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "MapCoords.h"

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
	// fld [T.x]; fmul 0.1; fstp float; fistp (round to nearest, the default FPU mode)
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
	return AltitudeAt(island, cell) >= 4; // 0x603682: cmp al, 4; sbb; inc
}

bool IsCoastal(const LandIslandInterface& island, glm::ivec2 cell)
{
	const auto* c = CellAt(island, cell);
	return c != nullptr && c->properties.hasWater == 0 && c->properties.coastLine != 0;
}

uint32_t CollideLandscape(const LandIslandInterface& island, glm::ivec2 cell)
{
	// fn_00601E00: the map cell is inside the game map (the same test as InBounds)
	if (!InBounds(island, cell))
	{
		return k_CollideEdge;
	}
	return IsWater(island, cell) ? k_CollideWater : k_CollideLand; // 0x601C7D: sete dl; inc edx
}

int32_t GetSurfaceType(const LandIslandInterface& island, glm::vec3 point)
{
	const auto cell = CellOf(point);
	const auto* c = CellAt(island, cell);
	if (c == nullptr)
	{
		return 6; // 0x71D950
	}
	if (c->properties.hasWater != 0)
	{
		return 7; // 0x71D966: !IsLand
	}
	// Terrain::GetMaterialInfo (0x735330): country + altitude * 12 + 8, the second material of the cell's altitude
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	if (c->properties.country >= countries.size() || !Locator::infoConstants::has_value())
	{
		return 3;
	}
	const auto& country = countries[c->properties.country];
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(*c), 255);
	const auto material = country.materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 3;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	const auto surface = type < info.size() ? static_cast<int32_t>(info[type].surfaceSound) : 3;
	return surface >= 1 && surface <= 8 ? surface : 3; // 0x71D97A: <= 0 or >= 9 -> 3
}

float ScriptLandHeight(const LandIslandInterface& island, glm::vec3 point)
{
	// fmul [0x8AC404] (0.1) + __ftol: truncated, not the MapCoords high word
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
