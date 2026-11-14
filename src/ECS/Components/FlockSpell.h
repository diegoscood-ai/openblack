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

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// What a flock miracle keeps: its flock, how many animals it has made and should have made by now along the hand's
/// sweep, where the sweep last was, and the trail its caster's hand leaves while it makes them
struct FlockSpell
{
	entt::entity flock {entt::null}; ///< The Flock (components::Flock), new at InitWithPos
	int created {0};                 ///< The animals tried so far (a skipped one counts too)
	float emitted {0.0f};            ///< The emit accumulator (12 per second)
	/// The last spawn point: MapCoords x, z (6553.6 per metre, truncated) and the height above the land (metres)
	glm::ivec2 lastSpawn {0};
	float lastSpawnHeight {0.0f};
	uint32_t castEffect {0};   ///< (flying only) The cast effect, a PSys without a spell stepped every frame
	unsigned int castTurn {0}; ///< (openblack) the game turn of InitWithPos, for OPENBLACK_TEST_FLOCK_SHOT
};

} // namespace openblack::ecs::components
