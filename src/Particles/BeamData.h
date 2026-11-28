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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

// What the simple beam keeps on its atoms and its beams' collections (ParticleBeamRules.cpp), which a save writes
// (docs/wiki/physics/particle-rules.md#the-simple-beam).

namespace openblack::particles::beam
{

/// The bits of the particle allocator's fill, which the original's beam data holds where it has set nothing
inline constexpr uint32_t k_Unset = 0xCDCDCDCDu;

/// What a beam's atom keeps: the object it follows (none for a point), the game turn that object was last found
/// available (unset until it has one) and the two ends of its beams
struct Target
{
	entt::entity object {entt::null};
	uint32_t validatedTurn {k_Unset};
	glm::vec3 target {0.0f};
	glm::vec3 origin {0.0f};
};

/// What a beam's collection keeps: its number among its atom's beams, the newest 0, so that no two wiggle alike (a beam
/// that was not numbered is 0), the count of its atom's beams (the rule's NumBeams), and the key points it was last laid
/// through, from its start to its end (none before)
struct Ribbon
{
	int number {0};
	int beams {0};
	std::vector<glm::vec3> keys;
};

} // namespace openblack::particles::beam
