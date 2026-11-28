/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandWaterGlow.h"

#include <algorithm>

#include "3D/LandIslandInterface.h"
#include "ECS/SeaCells.h"
#include "Graphics/HandLight.h"

using namespace openblack;
using namespace openblack::graphics;

uint32_t hand_water_glow::Colour(uint32_t warmColour, float strength)
{
	// R + (255 - R) / 4, G + (128 - G) / 4 and B + (64 - B) / 4 with exact integer shifts, so each is
	// floor((3 c + target) / 4)
	const auto channel = [warmColour](uint32_t shift, uint32_t target) {
		const uint32_t c = (warmColour >> shift) & 0xFFu;
		return static_cast<uint32_t>((3 * static_cast<int32_t>(c) + static_cast<int32_t>(target)) >> 2) & 0xFFu;
	};
	// The alpha is truncated towards zero
	const int alpha = std::clamp(static_cast<int>(strength * static_cast<float>(k_MaxAlpha)), 0, static_cast<int>(k_MaxAlpha));
	return static_cast<uint32_t>(alpha) << 24 | channel(16, 255) << 16 | channel(8, 128) << 8 | channel(0, 64);
}

bool hand_water_glow::Shows(bool inTemple, float strength)
{
	return !inTemple && strength > k_MinimumStrength;
}

bool hand_water_glow::NearLowLand(glm::vec2 xz, const AltitudeAt& altitudeAt, int lastCell)
{
	// Truncated, then a signed division by 10: both towards zero
	const auto cellOf = [](float v) { return static_cast<int>(v) / 10; };
	const int x0 = std::clamp(cellOf(xz.x - k_Reach), 0, lastCell);
	const int z0 = std::clamp(cellOf(xz.y - k_Reach), 0, lastCell);
	const int x1 = cellOf(xz.x + k_Reach);
	const int z1 = cellOf(xz.y + k_Reach);
	for (int z = z0; z <= z1; ++z)
	{
		for (int x = x0; x <= x1; ++x)
		{
			const auto altitude = altitudeAt(x, z);
			if (!altitude.has_value() || *altitude < k_LowAltitude)
			{
				return true;
			}
		}
	}
	return false;
}

bool hand_water_glow::NearLowLand(const LandIslandInterface& island, glm::vec2 xz)
{
	const auto altitudeAt = [&island](int x, int z) -> std::optional<uint16_t> {
		const auto* cell = ecs::sea_cells::CellAt(island, glm::ivec2(x, z));
		if (cell == nullptr)
		{
			return std::nullopt;
		}
		return island.GetCellAltitude(*cell);
	};
	return NearLowLand(xz, altitudeAt, std::max(0, static_cast<int>(island.GetCellsPerSide()) - 1));
}

std::optional<hand_water_glow::Quad> hand_water_glow::Compute(const LandIslandInterface& island, const glm::vec3& landColour,
                                                              uint32_t warmColour, const glm::vec3& handPosition)
{
	const float strength = HandLight::GetStrength(landColour);
	// The renderer draws it only outside the temple, where the sea is
	if (!Shows(false, strength) || !NearLowLand(island, glm::vec2(handPosition.x, handPosition.z)))
	{
		return std::nullopt;
	}
	return Quad {glm::vec2(handPosition.x, handPosition.z), Colour(warmColour, strength)};
}
