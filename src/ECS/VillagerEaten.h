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

namespace openblack::ecs
{
namespace components
{
struct LivingAction;
}

/// A villager a predator brought down (ECS/AnimalPredators.cpp downs it), through its own two states, as every villager
/// state runs (docs/wiki/beings/villager-reactions.md#hunted-villagers):
/// - DOWNED: the clip of its fall plays to its end (through WAIT_FOR_ANIMATION), then it is eaten, its turns until the
///   next change set to the turns of being eaten;
/// - BEING_EATEN: each turn counts one down (ECS/BeingEatenRules.h); at their end one that cannot be eaten plays its
///   clip to its end and gets up (LANDED), the others die, killed by an animal, their death weighing the life they had.
/// Either way out of those states the villager is no longer caught. Always 1
uint32_t VillagerDownedState(components::LivingAction& action);
uint32_t VillagerBeingEatenState(components::LivingAction& action);

} // namespace openblack::ecs
