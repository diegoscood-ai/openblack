/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Common/Zoomer.h"

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
	/// +0x5E: MobileWallHug's move state (1 ARRIVED, 4 FINAL_STEP, 5 WANDER, 0xB STEP_THROUGH, 0xC..0x12 the circle hug of
	/// ECS/AnimalWallHug.h; dev\tmp_dis\animals\wallhug.md)
	uint8_t moveState {0};
	/// +0x70 CircleHugInfo: the collide circle it walks round (a copy of the NewCollide::Obj: centre and radius, the
	/// fixed object it belongs to, none for a water cell's; set = GetObjectPtr() != NULL), +0x74 TurnsToObj (the turns
	/// until it reaches it, 0xFF none) and +0x76 the distance to the goal when the orbit began (x 128)
	struct HugCircle
	{
		glm::vec2 centre {0.0f};
		float radius {0.0f};
		entt::entity owner {entt::null};
		bool set {false};
	};
	HugCircle hugCircle;
	uint8_t turnsToObj {0xFF};
	uint32_t hugGoalDistance {0};
	/// +0x58: the eat counter; also the corpse counter (GetNumTurnsToDieOver, 600 turns) in DEAD
	int16_t counter {0};
	/// +0xE4 / +0xE8 / +0xEA: the hunger, sleep and breed counters (ProcessNeeds)
	int16_t hunger {0};
	int16_t sleep {0};
	int16_t breed {0};
	/// +0x104 / +0x106: the sleep place, the flock's domain centre cell at creation; (0, 0) = none
	glm::u16vec2 sleepCell {0};
	/// +0xB4: bit 0 dying / dead, bits 4-5 the landType of the last landing (Animal::EndPhysics), 0x80 downed by a predator
	uint16_t status {0};
	/// +0x60: the hunting target (a predator's prey)
	entt::entity target {entt::null};
	/// +0xF4 / +0xF8: the cell (centre) of a prey seen, (0, 0) = none
	glm::vec2 preyCell {0.0f};
	/// +0xF0: the game turn the chase started (chaseTime)
	uint32_t chaseStart {0};
	/// +0xFC: what it eats (a predator's downed prey)
	entt::entity foodTarget {entt::null};
	/// +0xBC: the object of its reaction (the predator, the food, the thrown object); +0x94 the reaction (an id of
	/// ECS/Effects/Reactions' list, 0 none); +0x8E the state to go back to (StorePreviousState: its final state)
	entt::entity predator {entt::null};
	uint32_t reaction {0};
	uint8_t previousState {0};
	/// +0x98: the reaction records are the Living's components::ReactionRecords (ECS/Effects/Reactions)
	/// +0x1C: the MapCoords altitude, metres above the land (the birds; 0 on the ground) and the goal's (+0x88)
	float altitude {0.0f};
	float goalAltitude {0.0f};
	/// +0x110..+0x13C: the bank zoomer (roll in radians; Dove::Draw rolls the drawn matrix by it)
	Zoomer bank;
	/// +0x148: the SpellWolf's final destination (SetRunToFinalDest); set by the spell
	glm::vec2 finalDestination {0.0f};
	/// +0xA0: the turn it was born (Living::GetAge = (turn - it) / 1500; there is no stored age). Negative for the
	/// animals a map creates already grown (SetAge at turn 0).
	int32_t birthTurn {0};
	/// the ground covered in the last turn (Object::IsMoving: the clip advances by distance while it moves)
	float movedLastTurn {0.0f};
};

/// A villager downed by a predator (fn_005EC480: status 0x80, life 0.05): DOWNED, then BEING_EATEN for 300 turns, then
/// dead (Villager::BeingEaten 0x76B380); the animal AI drives it (ECS/AnimalPredators.cpp)
struct DownedVillager
{
	int16_t counter {0};
};

} // namespace openblack::ecs::components
