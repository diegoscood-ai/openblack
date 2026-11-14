/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TerrainMaterial.h"

#include <cstring>

#include <algorithm>

#include <LNDFile.h>
#include <glm/vec3.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Weather/Weather.h"
#include "Locator.h"
#include "Magic/Core/Spell.h"

using namespace openblack;

namespace
{
const LandIslandInterface* Island()
{
	return Locator::terrainSystem::has_value() ? &Locator::terrainSystem::value() : nullptr;
}
} // namespace

uint32_t ecs::terrain_material::AtCell(glm::u16vec2 cell)
{
	return AtCell(Island(), cell);
}

uint32_t ecs::terrain_material::AtCell(const LandIslandInterface* island, glm::u16vec2 cell)
{
	const glm::vec3 corner(static_cast<float>(cell.x) * 10.0f, 0.0f, static_cast<float>(cell.y) * 10.0f);
	if (weather::GetSnowAt(magic::ToWorld(corner)) >= 27.0f)
	{
		return 27;
	}
	uint32_t material = 0;
	if (HasLandBlock(island, cell))
	{
		const auto& landCell = island->GetCell(cell);
		const auto& countries = island->GetCountries();
		const auto& materials = island->GetMaterialInfo();
		if (landCell.properties.country < countries.size())
		{
			const auto& country = countries[landCell.properties.country];
			const auto altitude = std::min<uint16_t>(island->GetCellAltitude(landCell), 255);
			const auto index = country.materials[altitude].indices[1];
			if (index < materials.size())
			{
				material = materials[index].type;
			}
		}
	}
	return material != 0 ? material : 1;
}

bool ecs::terrain_material::HasLandBlock(glm::u16vec2 cell)
{
	return HasLandBlock(Island(), cell);
}

bool ecs::terrain_material::HasLandBlock(const LandIslandInterface* island, glm::u16vec2 cell)
{
	if (island == nullptr || cell.x > 0x1FF || cell.y > 0x1FF)
	{
		return false;
	}
	if (cell.x >= island->GetCellsPerSide() || cell.y >= island->GetCellsPerSide())
	{
		return false;
	}
	// A cell outside every block reads as the island's empty cell
	lnd::LNDCell empty {};
	empty.properties.unused6 = true;
	return std::memcmp(&island->GetCell(cell), &empty, sizeof(empty)) != 0;
}
