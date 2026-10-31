/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArenaRing.h"

#include "Common/GUtilsX87.h"

using namespace openblack;

namespace
{
/// A whole turn, as the game's float keeps it
constexpr float k_WholeTurn = 6.2831855f;
} // namespace

float creature_arena_ring::Height(float radius)
{
	return gutils::x87::Store(gutils::x87::Mul(radius, k_HeightShare));
}

std::vector<glm::vec2> creature_arena_ring::Points(glm::vec2 centre, float radius, uint32_t count)
{
	namespace x87 = gutils::x87;
	std::vector<glm::vec2> points;
	points.reserve(count);
	for (uint32_t i = 0; i < count; ++i)
	{
		const double angle = x87::Div(x87::Mul(static_cast<double>(i), k_WholeTurn), x87::Sub(static_cast<double>(count), 1.0));
		const double cosine = x87::Store(x87::Cos(angle));
		const auto sine = x87::Sin(angle);
		points.emplace_back(x87::Store(x87::Add(x87::Store(x87::Mul(cosine, radius)), centre.x)),
		                    x87::Store(x87::Add(x87::Mul(sine, static_cast<double>(radius)), centre.y)));
	}
	return points;
}
