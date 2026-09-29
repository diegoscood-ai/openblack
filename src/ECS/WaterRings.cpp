/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaterRings.h"

using namespace openblack::ecs;

namespace
{
constexpr size_t k_MaxRings = 1024;
constexpr uint32_t k_RingLife = 700; // 0x2BC
std::vector<WaterRing> s_rings;
} // namespace

void openblack::ecs::AddWaterRing(const WaterRing& ring)
{
	if (s_rings.size() < k_MaxRings)
	{
		s_rings.push_back(ring);
	}
}

void openblack::ecs::UpdateWaterRings(float gameMilliseconds)
{
	for (auto& ring : s_rings)
	{
		// g_game_time_inc x rate, truncated (__ftol)
		ring.age += static_cast<uint32_t>(gameMilliseconds * ring.rate);
	}
	std::erase_if(s_rings, [](const WaterRing& ring) { return ring.age >= k_RingLife; });
}

const std::vector<WaterRing>& openblack::ecs::GetWaterRings()
{
	return s_rings;
}
