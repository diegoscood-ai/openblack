/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellGrid.h"

#include <array>

using namespace openblack::magic;

namespace
{
constexpr int k_Side = 64;
std::array<std::array<uint8_t, k_Side>, k_Side> g_Grid {};
uint32_t g_Decay = 0x20; ///< *(u32*)0xC22570

/// the MapCoords' cell (x >> 16) >> 3 (8 map cells of 10 m)
bool CellOf(const glm::vec3& position, int& x, int& z)
{
	if (position.x < 0.0f || position.z < 0.0f)
	{
		return false;
	}
	x = static_cast<int>(position.x / 10.0f) >> 3;
	z = static_cast<int>(position.z / 10.0f) >> 3;
	return x < k_Side && z < k_Side;
}
} // namespace

void spell_grid::Mark(const glm::vec3& position, uint8_t value)
{
	int x = 0;
	int z = 0;
	if (CellOf(position, x, z))
	{
		g_Grid[static_cast<size_t>(x)][static_cast<size_t>(z)] = value;
	}
}

void spell_grid::Decay()
{
	for (auto& column : g_Grid)
	{
		for (auto& cell : column)
		{
			if (cell != 0)
			{
				cell = cell <= g_Decay ? 0 : static_cast<uint8_t>(cell - g_Decay);
			}
		}
	}
	g_Decay = 0x20;
}

uint8_t spell_grid::At(const glm::vec3& position)
{
	int x = 0;
	int z = 0;
	return CellOf(position, x, z) ? g_Grid[static_cast<size_t>(x)][static_cast<size_t>(z)] : 0;
}

void spell_grid::Clear()
{
	for (auto& column : g_Grid)
	{
		column.fill(0);
	}
	g_Decay = 0x20;
}
