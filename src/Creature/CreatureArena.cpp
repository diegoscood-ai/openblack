/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArena.h"

#include <algorithm>

#include "Common/GameDistance.h"

using namespace openblack;
using namespace openblack::creature_arena;

namespace
{
/// A creature's running pace: its running share of its top speed, within 0 and 1, times its size's pace, with a tenth
/// to spare
constexpr float k_PaceSizeShare = 0.5f * 1.75f;
constexpr float k_PaceBase = 0.25f;
constexpr float k_PacePerSize = 20.0f;
constexpr float k_PaceMargin = 1.1f;
} // namespace

float creature_arena::ReuseDistance(float runShare, float size)
{
	const float pacePerShare = ((size * k_PaceSizeShare) + k_PaceBase) * k_PacePerSize;
	const float pace = std::clamp(runShare, 0.0f, 1.0f) * pacePerShare * k_PaceMargin;
	return pace * k_ReusePerPace;
}

std::optional<Placed> creature_arena::Place(float wantedRadius, const FindClearArea& find)
{
	for (float scale = 1.0f; scale > k_SmallestScale; scale -= k_ScaleStep)
	{
		const float radius = scale * wantedRadius;
		if (const auto place = find(radius + radius))
		{
			return Placed {.centre = *place, .radius = radius, .scale = scale};
		}
	}
	return std::nullopt;
}

std::optional<size_t> creature_arena::Nearest(std::span<const glm::ivec2> arenas, glm::ivec2 point, float within)
{
	std::optional<size_t> nearest;
	for (size_t i = 0; i < arenas.size(); ++i)
	{
		const float distance = game_math::GetDistanceInMetres(point, arenas[i]);
		if (distance < within)
		{
			nearest = i;
			within = distance;
		}
	}
	return nearest;
}
