/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TotemRing.h"

#include <cmath>

using namespace openblack::worship;

namespace
{
/// Point i stands at the angle i x k_Turn x k_NinthOfATurn
constexpr float k_Turn = 6.2831855f;
constexpr float k_NinthOfATurn = 0.11111111f;
constexpr float k_FullPulse = 255.0f;
} // namespace

std::array<glm::vec3, totem_ring::k_Points> totem_ring::Points(const glm::vec3& centre)
{
	std::array<glm::vec3, k_Points> points {};
	for (size_t i = 0; i < k_Points; ++i)
	{
		const float angle = (static_cast<float>(i) * k_Turn) * k_NinthOfATurn;
		const auto along = static_cast<float>(std::cos(static_cast<double>(angle))) * k_Radius;
		const auto across = static_cast<float>(std::sin(static_cast<double>(angle))) * k_Radius;
		// the point (along, 0, across) moved to the middle of the dance
		points.at(i) = glm::vec3(along + centre.x, 0.0f + centre.y, across + centre.z);
	}
	return points;
}

float totem_ring::Strength(float stored, float most)
{
	const float share = stored / most;
	if (!(share > 0.0f))
	{
		return 0.0f;
	}
	return share < 1.0f ? share : 1.0f;
}

uint32_t totem_ring::Colour(uint32_t playerColour, bool overloaded, float pulse)
{
	uint32_t colour = playerColour;
	if (overloaded)
	{
		// the pulse cut to a whole 0..255, each channel's product kept to its own byte before the shift down
		const auto scale = static_cast<uint32_t>(static_cast<int32_t>(pulse * k_FullPulse)) & 0xFFu;
		colour = ((((colour & 0xFF00u) * scale) & 0xFF0000u) | (((colour & 0xFFu) * scale) & 0xFF00u) |
		          (((colour & 0xFF0000u) * scale) & 0xFF0000FFu)) >>
		             8u |
		         (colour & 0xFF000000u);
	}
	return colour & 0xFFFFFFu;
}
