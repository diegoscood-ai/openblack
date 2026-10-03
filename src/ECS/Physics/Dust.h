/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <glm/vec3.hpp>

namespace openblack::ecs::physics
{
/// The dust puffs of the physics (fn_845C20 and the ground impacts of AttemptToAddSoundEvent 0x6464F0): smoke cells of
/// data\blobs.raw (rows 2-3 of the 8 x 8 sheet), a camera-facing alpha-blended quad of constant colour that grows in
/// 0.125 s and shrinks to nothing at 1 s, moving at its velocity with no gravity. Game time: they stop while paused.
class Dust
{
public:
	/// argb as in the original (0xAARRGGBB); size is the quad's half size at its largest.
	static void Emit(glm::vec3 at, glm::vec3 velocity, uint32_t argb, float size);
	/// The velocity of a ground impact's puffs (AttemptToAddSoundEvent 0x6467D1..0x646833): (LocalRand(201) - 100) x
	/// 0.02 on each axis, +-2 units per second, drawn z, y, x.
	[[nodiscard]] static glm::vec3 RandomVelocity();
	/// The same on the synced stream, a fragment's puffs (Fragment::SetUpPhysOb 0x76EEC5..0x76EF53): GameRand(201).
	[[nodiscard]] static glm::vec3 SyncedRandomVelocity();
	static void Update(float seconds);
	static void Clear();
	Dust() = delete;
};
} // namespace openblack::ecs::physics
