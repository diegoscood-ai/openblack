/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandBalance.h"

#include <array>

namespace openblack::land_balance
{
namespace
{
std::array<float, k_Count> g_Values = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
float g_LostTownScale = 1.0f;
} // namespace

void Reset()
{
	g_Values.fill(1.0f);
	g_LostTownScale = 1.0f; // 0x5E28A9
}

void Set(int index, float value)
{
	if (index >= 0 && static_cast<size_t>(index) < k_Count)
	{
		g_Values.at(static_cast<size_t>(index)) = value;
	}
}

float Get(size_t index)
{
	return index < k_Count ? g_Values.at(index) : 1.0f;
}

void SetLostTownScale(float scale)
{
	g_LostTownScale = scale;
}

float LostTownScale()
{
	return g_LostTownScale;
}

} // namespace openblack::land_balance
