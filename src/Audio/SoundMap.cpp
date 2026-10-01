/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SoundMap.h"

#include <cmath>
#include <cstring>

#include <algorithm>

#include <LNDFile.h>

#include "3D/LandIslandInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

int32_t openblack::audio::GetSurfaceType(glm::vec3 position)
{
	if (!Locator::terrainSystem::has_value())
	{
		return 6;
	}
	auto& island = Locator::terrainSystem::value();
	// MapCoords from an LHPoint: the cell is the 16.16 coordinate x 0.1 (10 units per cell); the grid is 512 cells (more
	// on the editor's bigger maps)
	const auto cx = static_cast<int32_t>(std::floor(position.x / 10.0f));
	const auto cz = static_cast<int32_t>(std::floor(position.z / 10.0f));
	const int32_t last = island.GetCellsPerSide() - 1;
	if (cx < 0 || cx > last || cz < 0 || cz > last)
	{
		return 6;
	}
	const auto& cell = island.GetCell(glm::u16vec2(cx, cz));
	// no land block there: LandIsland answers with its empty cell (all zero but fullWater)
	lnd::LNDCell empty {};
	empty.properties.fullWater = true;
	if (std::memcmp(&cell, &empty, sizeof(empty)) == 0)
	{
		return 6;
	}
	if (cell.properties.hasWater)
	{
		return 7;
	}
	// Terrain::GetMaterialInfo 0x735330: the second material of the cell's altitude in its country
	const auto& countries = island.GetCountries();
	const auto& materials = island.GetMaterialInfo();
	if (cell.properties.country >= countries.size())
	{
		return 3;
	}
	const auto& country = countries[cell.properties.country];
	const auto altitude = std::min<uint16_t>(island.GetCellAltitude(cell), 255);
	const auto material = country.materials[altitude].indices[1];
	if (material >= materials.size())
	{
		return 3;
	}
	const auto& info = Locator::infoConstants::value().terrainMaterial;
	const auto type = materials[material].type;
	const auto surface = type < info.size() ? static_cast<int32_t>(info[type].surfaceSound) : 3;
	return surface >= 1 && surface <= 8 ? surface : 3;
}
