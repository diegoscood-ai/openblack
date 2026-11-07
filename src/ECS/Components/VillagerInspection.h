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

/// On a villager inspecting a creature (ecs::villager_creature): the distance in metres it looks from and the turns
/// left of its look before it walks closer. Kept after the reaction ends, as the original keeps it, until the next
/// inspection sets it again
struct VillagerInspection
{
	float distance {0.0f};
	uint32_t turnsUntilApproach {0};
};

} // namespace openblack::ecs::components
