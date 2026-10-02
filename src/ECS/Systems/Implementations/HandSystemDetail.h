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
//   HandPhysics.cpp     trees, pots and stores in the physics system (ECS/Physics)
//   HandDebugHooks.cpp  environment-variable test hooks

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"
#include "Audio/Device/Sound.h"

namespace openblack::ecs::systems::hand_detail
{
/// GAudio::PlaySoundEffect(NULL, sample, 3, 0, 0, 0, bank) 0x429D60, a 2D one-shot without owner: the hand's failure
/// (FailApply fn_005D18F0 0x5D1941: G_SpellCastFailure). The sample is a "<bank>.sad/<n>" SoundId.
void PlaySample(audio::SoundId id);
/// GAudio::PlaySoundEffect(LH_SamplePlayOptions*) 0x429E30 with is3D 1, track 0, no owner, at a world point (the
/// options form of the hand's 3D sites, e.g. G_HandInWater fn_005D1AB0 0x5D2167): not started beyond the sample's max
/// distance from the camera. Returns the channel as an entity (entt::null when it did not start).
entt::entity PlaySample3D(audio::SoundId id, glm::vec3 point);
/// MapCoords::IsLand (0x603720): the landscape cell under the point does not have the water bit.
bool IsLand(glm::vec3 point);
/// OPENBLACK_DUMP_ENTITY_COUNTS (HandDebugHooks.cpp): entity counts per kind, once.
void DumpEntityCounts();
} // namespace openblack::ecs::systems::hand_detail
