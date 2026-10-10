/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <glm/vec3.hpp>

#include "Particles/SoundAction.h"

// The weather's fork lightning: a storm queues the point under its cloud, and UR_LightningStrike (the root rule of the
// always-on SF_LightningStrike utility effect) empties the queue once a turn, newest first, into one strike atom per
// point. The queue and the count of live strikes are in the particle system's state (psys::manager::State). Wiki:
// docs/bw1-notes/miracles.md, "The weather's fork strike".

namespace openblack::psys::lightning_strike
{

/// One queued strike: the point, and the storm's outer radius, which is kept as the original keeps it but never read
struct Queued
{
	glm::vec3 point {0.0f};
	float radius {0.0f};
};

/// A strike lives 0.4 + rand(0.4) seconds
constexpr float k_MinLife = 0.4f;
constexpr float k_MaxLife = 0.8f;
/// A queued point is dropped while this many strikes are alive
constexpr uint32_t k_MaxStrikes = 50;

/// The strike's sound size from its draw r in 0..1: below 0.3 small (3), below 0.75 medium (2), else large (1). A NaN
/// counts as below both, so it is small
[[nodiscard]] constexpr int32_t StrikeSoundSize(float r)
{
	if (!(r >= 0.3f))
	{
		return 3;
	}
	if (!(r >= 0.75f))
	{
		return 2;
	}
	return 1;
}

/// The rule's SOUND_SPELL_LIGHTNING for one strike: its size from the draw r, played delayed (it waits for the sound to
/// travel to the camera) and on the land under the point
[[nodiscard]] constexpr SoundAction StrikeSound(SoundAction action, float r)
{
	action.size = StrikeSoundSize(r);
	action.flags = static_cast<uint8_t>(action.flags | SoundAction::k_Delayed | SoundAction::k_SnapToGround);
	return action;
}

/// The point at the end of the queue; nothing without the particle system
void Queue(const glm::vec3& point, float radius);
/// Empties the queue (a new land); the live strikes go with their atoms
void Clear();
/// How many points wait; 0 without the particle system
[[nodiscard]] size_t QueuedCount();

} // namespace openblack::psys::lightning_strike
