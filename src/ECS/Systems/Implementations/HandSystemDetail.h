/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// Helpers shared by the HandSystem translation units:
//   HandSystem.cpp      input state machine, animation, public interface
//   HandPlacement.cpp   hand geometry, placement, cursor pick (ObtainRequiredHandPosition)
//   HandHolding.cpp     pick up, hold poses, spring, drop / throw
//   HandResources.cpp   piles, pots, multi pick-up, put down, stores
//   HandTrees.cpp       tug, uproot, roots, replant, dead trees
//   HandEffects.cpp     grip dust, multi pick-up particles
//   HandFish.cpp        splash of gripping the water, catching fish
//   HandDebugHooks.cpp  environment-variable test hooks

#include <glm/vec3.hpp>

#include "Audio/AudioManagerInterface.h"

namespace openblack::ecs::systems::hand_detail
{
/// Plays a one-shot sample if it is loaded.
void PlaySample(audio::SoundId id);
/// MapCoords::IsLand (0x603720): the landscape cell under the point does not have the water bit.
bool IsLand(glm::vec3 point);
} // namespace openblack::ecs::systems::hand_detail
