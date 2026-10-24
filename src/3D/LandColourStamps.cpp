/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LandColourStamps.h"

#include <cmath>

#include <algorithm>

namespace openblack::land_colour_stamps
{

namespace
{
constexpr float k_CellScale = 0.1f;

/// A cell along one axis, rounded towards zero and then down, and the weight of its texel, rounded to the nearest, in
/// floats
int CellAndWeight(float f, int& cell)
{
	cell = static_cast<int>(f);
	int weight = 0;
	if (!(f < 0.0f))
	{
		const float fraction = f - static_cast<float>(cell);
		const float scaled = fraction * 255.0f;
		weight = static_cast<int>(std::nearbyint(255.0f - scaled));
	}
	else
	{
		const float fraction = static_cast<float>(cell) - f;
		weight = static_cast<int>(std::nearbyint(fraction * 255.0f));
		--cell;
	}
	return weight & 0xFF;
}

/// A texel of the lightning's glow, from its centre: (32 - d) x 9, truncated and capped at 255, inside d < 32
uint8_t LightningTexel(int x, int y)
{
	const float d = std::sqrt(static_cast<float>(x * x + y * y));
	if (!(d < 32.0f))
	{
		return 0;
	}
	const int value = static_cast<int>((32.0f - d) * 9.0f);
	return static_cast<uint8_t>(std::min(value, 255));
}
} // namespace

Placement Place(glm::vec2 xz)
{
	Placement placement {};
	placement.weight.y = CellAndWeight(xz.y * k_CellScale, placement.cell.y);
	placement.weight.x = CellAndWeight(xz.x * k_CellScale, placement.cell.x);
	return placement;
}

glm::vec2 CentredCorner(const glm::vec3& centre, int32_t side)
{
	const float offset = static_cast<float>(side - 1) * 5.0f;
	return {centre.x - offset, centre.z - offset};
}

uint8_t Strength(float strength)
{
	// Below 0 is 0, above 255 is 255
	float scaled = strength * 255.0f;
	if (scaled < 0.0f)
	{
		scaled = 0.0f;
	}
	else if (scaled > 255.0f)
	{
		scaled = 255.0f;
	}
	return static_cast<uint8_t>(static_cast<int>(scaled));
}

std::vector<uint8_t> LightningImage()
{
	// Rows of 64 texels of 3 bytes, the three channels equal
	std::vector<uint8_t> texels(static_cast<size_t>(k_LightningSide) * k_LightningSide * 3);
	for (int row = 0; row < k_LightningSide; ++row)
	{
		for (int column = 0; column < k_LightningSide; ++column)
		{
			const uint8_t value = LightningTexel(row - k_LightningSide / 2, column - k_LightningSide / 2);
			const auto at = (static_cast<size_t>(row) * k_LightningSide + static_cast<size_t>(column)) * 3;
			texels[at] = value;
			texels[at + 1] = value;
			texels[at + 2] = value;
		}
	}
	return texels;
}

} // namespace openblack::land_colour_stamps
