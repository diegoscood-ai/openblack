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

#include <glm/vec2.hpp>

namespace openblack::ecs::components
{

/// The Living / MobileWallHug / Animal fields of an animal that its per-turn AI uses (ECS/AnimalAI.h; research
/// dev\tmp_dis\animals\grazing_ai.md and hand_death.md). Positions and steps in metres, speed and angles in the
/// original's units.
struct AnimalBrain
{
	/// +0x8C / +0x8D: the top state and the final (destination) state, AnimalStates (0..52)
	uint8_t topState {43};
	uint8_t finalState {43};
	/// +0x90 TurnsSinceStateChange (reset when the top state is set)
	uint16_t turnsSinceStateChange {0};
	/// +0x5A: the speed in MapCoords per turn (6553.6 per metre; SpeedState / 10)
	uint16_t speed {0};
	/// +0x5C: the heading, 2048 per circle; the step goes along (COS[a], SIN[a]) in x, z
	uint16_t angle {0};
	/// +0x64 / +0x6C: the per-turn step in MapCoords (WANDER's straight line)
	glm::ivec2 step {0};
	/// +0x80: MOVE_TO_POS's goal (metres)
	glm::vec2 goal {0.0f};
	/// +0x58: the eat counter; also the corpse counter (GetNumTurnsToDieOver, 600 turns) in DEAD
	int16_t counter {0};
	/// +0xE4 / +0xE8 / +0xEA: the hunger, sleep and breed counters (ProcessNeeds)
	int16_t hunger {0};
	int16_t sleep {0};
	int16_t breed {0};
	/// +0x104 / +0x106: the sleep place, the flock's domain centre cell at creation; (0, 0) = none
	glm::u16vec2 sleepCell {0};
	/// +0xB4: bit 0 dying / dead, bits 4-5 the landType of the last landing (Animal::EndPhysics)
	uint16_t status {0};
	/// the ground covered in the last turn (Object::IsMoving: the clip advances by distance while it moves)
	float movedLastTurn {0.0f};
};

} // namespace openblack::ecs::components
