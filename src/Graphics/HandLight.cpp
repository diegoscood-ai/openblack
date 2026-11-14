/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandLight.h"

#include <cmath>

#include <algorithm>

namespace openblack::graphics
{

namespace
{
/// The light comes up as the mean falls from 120 to 105: a fifteenth of full strength for each whole value under 120
[[nodiscard]] float StrengthOfMean(int32_t mean)
{
	if (mean >= 120)
	{
		return 0.0f;
	}
	return std::min(static_cast<float>(120 - mean) * (1.0f / 15.0f), 1.0f);
}

/// The mean of three bytes, rounded down
[[nodiscard]] int32_t MeanOf(int32_t r, int32_t g, int32_t b)
{
	return (r + g + b) / 3;
}

[[nodiscard]] int32_t ToByte(float channel)
{
	return static_cast<int32_t>(std::floor(channel * 255.0f + 0.5f));
}
} // namespace

glm::vec2 HandLight::GetOrigin(glm::vec3 handPosition)
{
	constexpr auto k_HalfWidth = static_cast<float>(k_Size - 1) * k_Spacing * 0.5f;
	return {handPosition.x - k_HalfWidth, handPosition.z - k_HalfWidth};
}

float HandLight::GetStrength(uint32_t landColour)
{
	const auto channel = [landColour](uint32_t shift) { return static_cast<int32_t>((landColour >> shift) & 0xFFu); };
	return StrengthOfMean(MeanOf(channel(16), channel(8), channel(0)));
}

float HandLight::GetStrength(const glm::vec3& landColour)
{
	return StrengthOfMean(MeanOf(ToByte(landColour.r), ToByte(landColour.g), ToByte(landColour.b)));
}

} // namespace openblack::graphics
