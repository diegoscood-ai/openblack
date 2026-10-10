/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaterRings.h"

namespace openblack::water_rings
{

bool Advance(Ring& ring, float milliseconds)
{
	// The step is cut to whole milliseconds, towards zero
	ring.age += static_cast<uint32_t>(milliseconds * ring.rate);
	return ring.age < k_Life;
}

} // namespace openblack::water_rings
