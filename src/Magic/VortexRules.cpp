/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VortexRules.h"

#include <cmath>

#include <algorithm>
#include <numeric>

namespace openblack::vortex
{
namespace
{
/// A map cell's side, and its inverse as the game multiplies by it
constexpr float k_CellSize = 10.0f;
constexpr float k_CellsPerUnit = 0.1f;

/// Eased in and out: 0 at 0, 1 at 1, flat at both ends
float Smooth(float x)
{
	return (3.0f - (x + x)) * x * x; // ((3 - 2x) x) x
}
} // namespace

float Openness(VortexStateType state, float seconds)
{
	switch (state)
	{
	case VortexStateType::Inactive:
		return 0.0f;
	case VortexStateType::Active:
		return 1.0f;
	case VortexStateType::FadeIn:
		if (seconds < k_FadeDelaySeconds)
		{
			return 0.0f;
		}
		return Smooth(std::clamp((seconds - k_FadeDelaySeconds) / k_FadeSeconds, 0.0f, 1.0f));
	default: // FadeOut (any other state)
		return seconds < k_FadeSeconds ? Smooth(1.0f - seconds / k_FadeSeconds) : 0.0f;
	}
}

float LevelAmount(VortexStateType state, float seconds)
{
	const float s = state == VortexStateType::FadeOut ? 1.0f : Openness(state, seconds);
	const float r = 1.0f - s;
	return 1.0f - r * r;
}

float GlowBrightness(VortexStateType state, float seconds)
{
	// a fraction kept to 0..1; one not above 0 (a NaN too) is 0
	const auto unit = [](float c) { return !(c > 0.0f) ? 0.0f : (c < 1.0f ? c : 1.0f); };
	switch (state)
	{
	case VortexStateType::Inactive:
		return 0.0f;
	case VortexStateType::Active:
		return 0.6f;
	case VortexStateType::FadeIn:
		if (seconds < 2.0f)
		{
			return seconds / 2.0f;
		}
		return (0.6f - 1.0f) * unit((seconds - 2.0f) / 5.0f) + 1.0f;
	default: // FadeOut (any other state)
		if (seconds < 5.0f)
		{
			return (1.0f - 0.6f) * (seconds / 5.0f) + 0.6f;
		}
		return 1.0f - unit((seconds - 5.0f) / 2.0f);
	}
}

uint8_t GroundHoleThreshold(float openness)
{
	// compared unsigned, so a negative product is capped too
	constexpr auto k_Max = static_cast<uint32_t>(k_GroundHoleMaxThreshold);
	const auto ref = static_cast<uint32_t>(static_cast<int32_t>(openness * 255.0f));
	return static_cast<uint8_t>(ref < k_Max ? ref : k_Max);
}

float SwirlDepth(float levelAmount)
{
	return (0.3f - 2.5f) * levelAmount + 2.5f;
}

glm::ivec2 CentreCell(glm::vec2 centre)
{
	return {static_cast<int>(centre.x * k_CellsPerUnit), static_cast<int>(centre.y * k_CellsPerUnit)};
}

glm::ivec2 SquareCell(glm::ivec2 centreCell, size_t index)
{
	const auto x = static_cast<int>(index / k_LevelSide);
	const auto z = static_cast<int>(index % k_LevelSide);
	return centreCell - glm::ivec2(k_LevelHalfSide) + glm::ivec2(x, z);
}

float AverageAltitude(std::span<const uint8_t> altitudes)
{
	if (altitudes.empty())
	{
		return 0.0f;
	}
	// Summed as the game sums them, in a float
	const auto sum = std::accumulate(altitudes.begin(), altitudes.end(), 0.0f,
	                                 [](float total, uint8_t altitude) { return total + static_cast<float>(altitude); });
	return sum / static_cast<float>(altitudes.size());
}

float LevelWeight(float distance)
{
	if (distance > k_LevelOuterRadius)
	{
		return 0.0f;
	}
	if (distance < k_LevelFullRadius)
	{
		return 1.0f;
	}
	return (k_LevelOuterRadius - distance) / (k_LevelOuterRadius - k_LevelFullRadius);
}

uint8_t LevelledAltitude(uint8_t original, float average, float distance, float amount)
{
	// The pull towards the average, whole within the inner radius and falling away in a straight line to the outer. The
	// game also adds a height profile by distance, but every point of it is 0 with the game's data, so it never changes
	// the land. Each step is rounded to a float, as the game's are: the pull, its share by the amount, then the height.
	const auto start = static_cast<float>(original);
	float pull = 0.0f;
	if (distance <= k_LevelOuterRadius)
	{
		pull = distance < k_LevelFullRadius
		           ? average - start
		           : (k_LevelOuterRadius - distance) * (average - start) / (k_LevelOuterRadius - k_LevelFullRadius);
	}
	const float height = pull * amount + start;
	// Rounded to the nearest, halves to even, and kept to a cell's heights
	return static_cast<uint8_t>(std::clamp(static_cast<int>(std::nearbyint(height)), 0, 255));
}

std::array<uint8_t, k_LevelCellCount> LevelSquare(glm::vec2 centre, std::span<const uint8_t> originals, float average,
                                                  float amount)
{
	std::array<uint8_t, k_LevelCellCount> heights {};
	const auto centreCell = CentreCell(centre);
	for (size_t i = 0; i < heights.size() && i < originals.size(); ++i)
	{
		const auto cell = SquareCell(centreCell, i);
		const auto dx = static_cast<float>(cell.x) * k_CellSize - centre.x;
		const auto dz = static_cast<float>(cell.y) * k_CellSize - centre.y;
		const auto distance = std::sqrt(dx * dx + dz * dz);
		heights.at(i) = LevelledAltitude(originals[i], average, distance, amount);
	}
	return heights;
}

} // namespace openblack::vortex
