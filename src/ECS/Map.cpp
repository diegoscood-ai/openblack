/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Map.h"

#include <glm/gtx/vec_swizzle.hpp>

#include "Locator.h"
#include "MapCoords.h"

using namespace openblack::ecs;

MapInterface::CellId MapInterface::GetGridCell(const glm::vec2& pos)
{
	// MapCoords(LHPoint) 0x603160 then the unsigned high words (ToMap 0x603433): a negative position is cell 0xFFFF and a
	// position past the map a cell >= 512, both off the map (InBounds 0x6042C0)
	return CellId(map_coords::CellOf(pos));
}

MapInterface::CellId MapInterface::GetGridCell(const glm::vec3& pos)
{
	return GetGridCell(glm::xz(pos));
}

glm::vec2 MapInterface::GetCellCenter(const MapInterface::CellId& cellId)
{
	return glm::vec2(cellId.x << 0x10, cellId.y << 0x10) / k_PositionToGridFactor + 5.0f;
}
