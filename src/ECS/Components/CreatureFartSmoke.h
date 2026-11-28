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

/// A creature's fart smoke (a ChimneySmoke on the creature) and the plan that made it: the smoke puffs at the anus
/// while the creature still carries that plan out
struct CreatureFartSmoke
{
	uint32_t planSerial {0};
};

} // namespace openblack::ecs::components
