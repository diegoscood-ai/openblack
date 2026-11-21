/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"

/// The Japanese totem puzzle (PuzzleGame type 8) in the world: its six totems in an arc, its turn, which takes a totem
/// raised or lowered as a move and moves the others by the puzzle's table, and the test of its solution. The rules are
/// ecs::totem_puzzle (ECS/TotemPuzzleRules.h). Wiki: docs/wiki/world/totem-puzzle.md
namespace openblack::ecs::totem_puzzle_game
{

/// The puzzle at its start, made with it: six Japanese totems in an arc round its place, each of a highest step of 4,
/// sunk, unmovable and turned away from the middle, every kept height 0
void Start(entt::entity puzzle, const map_coords::MapCoords& place, float scale);

/// One turn: a totem that has gone is made again where it stood; the first totem whose height is no longer the one the
/// puzzle keeps is the move, which sets the kept heights by the table, puts every totem at its kept height (the others
/// blending to it) and puts a sparkle over each totem at its highest
void Process(entt::entity puzzle);

/// The test of a solution: every kept height at the highest. A solution takes the sparkles away and puts a sparkle of
/// three turns over each totem's place, and the puzzle is played
bool TestSolution(entt::entity puzzle);

/// A totem has been moved at least once
[[nodiscard]] bool Started(entt::entity puzzle);

} // namespace openblack::ecs::totem_puzzle_game
