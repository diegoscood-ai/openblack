/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::ecs::components
{

/// The weather at a point (8 bytes). Storms add to calm, temperature 0 air. The same layout is a grid cell of the
/// atmosphere, the tail of a storm and what the weather queries return.
struct WeatherInfo
{
	int8_t temperature {0}; ///< Degrees
	/// Rain intensity, percent: a signed byte, added with a clamp, 0..127 in practice (a storm adds 100 x strength)
	int8_t rain {0};
	/// Snowfall intensity, percent
	int8_t snow {0};
	/// Cloud cover, percent
	int8_t overcast {0};
	int8_t windX {0}; ///< x 1/8 = metres per second
	int8_t windZ {0};
	int8_t snowCover {0}; ///< The snow lying on the ground (half the land's snow cover)
	uint8_t stamp {0};    ///< Grid cells: the atmosphere frame the cell was computed in
};

} // namespace openblack::ecs::components
