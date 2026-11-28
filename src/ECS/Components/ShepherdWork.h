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

#include "Enums.h"

namespace openblack::ecs::components
{

/// A shepherd's two counters (ECS/Systems/Implementations/VillagerShepherd.h): the turns it waits in its present state, and the
/// state it goes on to once a wait for its flock is over, none for none
struct ShepherdWork
{
	int16_t waitTurns {0};
	VillagerStates pending {VillagerStates::InvalidState};
};

} // namespace openblack::ecs::components
