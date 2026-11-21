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

/// The two tree puzzles (PuzzleGame types 1 and 2) in the world: their grid of trees, their turn, which puts back and
/// replants what was taken, and the test of their solution. The rules are ecs::tree_puzzle (ECS/TreePuzzleRules.h).
/// Wiki: docs/wiki/world/tree-puzzles.md
namespace openblack::ecs::tree_puzzle_game
{

/// The puzzle at its start, made with it: a small tree on each spot, seven pulls of random spots, a lantern by each
/// corner, a puff of mist over each spot and a flock of nine bats over the middle one
void Start(entt::entity puzzle, const map_coords::MapCoords& place, float yAngleRadians, float scale);

/// One turn: a tree on its spot grows a step; a tree that went or moved off its spot counts as a pull of that spot
void Process(entt::entity puzzle);

/// The test of a solution: a tree on each spot at its full size and every spot in the same state. A solution sets the
/// trees alight with half their life, takes the mist away, lets the bats die and lets go of the trees, so the test
/// gives no again afterwards, as the original's
bool TestSolution(entt::entity puzzle);

/// A tree has been taken or moved off its spot at least once
[[nodiscard]] bool Started(entt::entity puzzle);

} // namespace openblack::ecs::tree_puzzle_game
