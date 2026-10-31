/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BookmarkPulse.h"

#include <cmath>

#include <algorithm>

namespace openblack::bookmark_pulse
{

float Wave(int32_t timeMs)
{
	const float turns = static_cast<float>(timeMs) * k_TurnsPerMs;
	const float angle = turns * k_TwoPi;
	// the cosine and the sum are kept wider and rounded to a float once, at the end
	return static_cast<float>(std::abs(std::cos(static_cast<double>(angle))) + static_cast<double>(k_Minimum));
}

Pulse Step(const Pulse& pulse, uint32_t frameMs)
{
	// the frame's ms are added as whole numbers that wrap, as the game's clock gives them
	auto timeMs = static_cast<int32_t>(static_cast<uint32_t>(pulse.timeMs) + frameMs);
	if (timeMs > k_PeriodMs)
	{
		timeMs %= k_PeriodMs;
	}
	return {
	    .timeMs = timeMs,
	    .width = Wave(timeMs),
	    .height = Wave((timeMs + k_HeightLeadMs) % k_PeriodMs),
	};
}

glm::vec2 MarkerHalfSize(const Pulse& pulse)
{
	const float stretch = pulse.height / pulse.width;
	const float size = std::max(pulse.width, k_SmallestSize);
	return {size, stretch * size};
}

} // namespace openblack::bookmark_pulse
