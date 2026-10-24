/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SkyFrame.h"

#include "Graphics/Moon.h"

namespace openblack::sky_frame
{

void TakeMoonPhase(ecs::components::Moon& moon, int64_t unixTime, uint32_t ticks)
{
	// The date is kept between reads, and read again once two seconds have gone by (or before it was ever read)
	if (static_cast<int32_t>(ticks - moon.dateReadAt) > k_MoonDateReadInterval)
	{
		moon.dateReadAt = ticks;
		moon.date = 0;
	}
	if (moon.date == 0)
	{
		moon.date = unixTime;
	}
	moon.phase = graphics::moon::Phase(MoonDate(moon));
}

int64_t MoonDate(const ecs::components::Moon& moon)
{
	return moon.dateOverride.value_or(moon.date);
}

} // namespace openblack::sky_frame
