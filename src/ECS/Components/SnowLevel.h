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

/// The snow an object shows, 0 to 255 (ecs::snow_levels). It moves a little towards the snow lying under the object
/// each time the land's draw draws it, and stays as it is while nothing draws it there. An object without it has
/// none yet: it is given the first time its draw finds snow under it
struct SnowLevel
{
	int32_t level {0};
};

/// The snow on a broken building's broken model, and on each piece that broke off it (ecs::snow_levels), 0 to 255. A
/// new broken model starts with the snow its building shows; a piece starts with its broken model's and holds it while
/// it flies or sticks; each draw of one that does not hold it moves it a step towards the snow lying under it
struct BrokenModelSnow
{
	int32_t level {0};
	/// a piece that has not landed yet: its snow stays as it broke off
	bool held {false};
};

} // namespace openblack::ecs::components
