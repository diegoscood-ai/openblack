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

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs::components
{

/// A piece of the beach temple puzzle (type 3): a base or a ring, a mobile object of its own. The puzzle puts it back
/// when it moves off its place, and puts a dropped ring on the stack it lands near (ecs/HanoiPuzzle.h)
struct HanoiBlock
{
	/// The puzzle it belongs to; none for a ring once the puzzle has gone
	entt::entity puzzle {entt::null};
	/// Its rest place on the map, with the altitude above the ground
	map_coords::MapCoords home {};
	/// Where the puzzle put it in the world: it is off its place once it is anywhere else
	glm::vec3 placedAt {0.0f};
	/// The piece that rests on it, and the piece it rests on
	entt::entity above {entt::null};
	entt::entity below {entt::null};
};

/// A PuzzleGame, made by the CHL CREATE / CREATE_WITH_ANGLE_AND_SCALE of type 32 (position, sub type, angle x 2048 /
/// 2 pi, scale). The fish puzzle (type 14) and the beach temple puzzle (type 3) are ported (ecs/PuzzleGames.h).
struct PuzzleGame
{
	script::PuzzleGameType type {script::PuzzleGameType::None};
	glm::vec3 position {0.0f}; ///< as a point: (x, altitude + y, z)
	int32_t angle {0};         ///< the 2048-step angle
	float scale {1.0f};
	bool played {false};                                         ///< once set the puzzle is not processed again
	entt::entity bait {entt::null};                              ///< type 14
	std::array<entt::entity, 2> shoals {entt::null, entt::null}; ///< type 14
	/// Type 3: the puzzle's place on the map, and the two steps of its grid in map units (the game's cosine and sine of
	/// its angle, times its size, doubled at the start)
	map_coords::MapCoords place {};
	int32_t cosineStep {0};
	int32_t sineStep {0};
	/// Type 3: a ring has gone onto a stack at least once (the puzzle's play has begun)
	bool begun {false};
	/// Type 3: the count of calm turns, in which no ring was off its place, from -5 to 5
	int32_t calmTurns {5};
	/// Type 3: the three bases and the four rings, the largest first
	std::array<entt::entity, 3> bases {entt::null, entt::null, entt::null};
	std::array<entt::entity, 4> rings {entt::null, entt::null, entt::null, entt::null};
	/// Type 3: the last state of each piece, the bases first, from which the puzzle makes again a piece that went
	std::array<HanoiBlock, 7> kept {};
};

} // namespace openblack::ecs::components
