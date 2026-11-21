/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureSpells.h"

namespace openblack::ecs::components
{

/// A creature's own smallest and largest size, where the small and big spells take it, as a script set them. A
/// creature without one has the defaults.
struct CreatureSizeLimits
{
	creature_spells::SizeLimits limits;
};

} // namespace openblack::ecs::components
