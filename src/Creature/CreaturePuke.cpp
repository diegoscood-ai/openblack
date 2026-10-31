/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePuke.h"

using namespace openblack;

uint32_t creature_puke::DropColour(uint32_t light, int32_t red, int32_t green, int32_t blue)
{
	const auto channel = [light](int shift, int32_t tint) {
		const auto lit = static_cast<int32_t>((light >> shift) & 0xFFu);
		return static_cast<uint32_t>((lit * tint / 255) & 0xFF) << shift;
	};
	// the alpha times the tint's full alpha
	return channel(24, 255) | channel(16, red) | channel(8, green) | channel(0, blue);
}

std::array<creature_puke::Drop, creature_puke::k_DropsPerTurn> creature_puke::DropsOf(const glm::vec3& mouth, float size,
                                                                                      uint32_t light, const LocalRandom& random)
{
	const float spread = size * k_Spread;
	const float offset = size * k_Offset;
	std::array<Drop, k_DropsPerTurn> drops {};
	for (auto& drop : drops)
	{
		const float x = static_cast<float>(random(100)) * spread - offset;
		const float y = -(static_cast<float>(random(100)) * spread);
		const float z = static_cast<float>(random(100)) * spread - offset;
		const int32_t red = random(100) + 80;
		const int32_t green = random(80) + 60;
		const int32_t blue = random(30) + 20;
		drop = {.position = mouth,
		        .velocity = glm::vec3(x, y, z),
		        .argb = DropColour(light, red, green, blue),
		        .size = size * k_SizeShare};
	}
	return drops;
}
