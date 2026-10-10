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
float StrengthOfMean(float mean)
{
	if (mean >= 120.0f)
	{
		return 0.0f;
	}
	return std::clamp((120.0f - mean) / 15.0f, 0.0f, 1.0f);
}
} // namespace

glm::vec2 HandLight::GetOrigin(glm::vec3 handPosition)
{
	constexpr auto k_HalfWidth = static_cast<float>(k_Size - 1) * k_Spacing * 0.5f;
	return {handPosition.x - k_HalfWidth, handPosition.z - k_HalfWidth};
}

float HandLight::GetStrength(uint32_t landColour)
{
	const auto channel = [landColour](uint32_t shift) { return static_cast<float>((landColour >> shift) & 0xFFu); };
	return StrengthOfMean((channel(16) + channel(8) + channel(0)) / 3.0f);
}

float HandLight::GetStrength(const glm::vec3& landColour)
{
	const float mean = (std::floor(landColour.r * 255.0f + 0.5f) + std::floor(landColour.g * 255.0f + 0.5f) +
	                    std::floor(landColour.b * 255.0f + 0.5f)) /
	                   3.0f;
	return StrengthOfMean(mean);
}

} // namespace openblack::graphics
