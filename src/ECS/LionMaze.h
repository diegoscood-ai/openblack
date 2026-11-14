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
#include "ScriptHeaders/ScriptEnums.h"

/// The walled mazes in the world: the lion maze (PuzzleGame type 12, Land 5's Stanley the wolf), its grid, the bells
/// round it, the wolf and the sheep, its turn, which takes a rung bell as the wolf's slide, and the test of its solution;
/// and the Theseus mazes (types 10 and 11, Land 4), whose villager a click on the grid moves a square with the wolf
/// after him, solved when he is out. The rules
/// are ecs::lion_maze (ECS/LionMazeRules.h). Wiki: docs/wiki/world/lion-maze.md
namespace openblack::ecs::lion_maze_game
{

/// The puzzle at its start, made with it: the walls its map gives, a palm at each corner, a lantern half way along each
/// side, the wolf and the sheep (a Theseus maze's villager) on their squares, then the lion maze's five bells, rung only
/// by a tap
void Start(entt::entity puzzle, const map_coords::MapCoords& place, float yAngleRadians, float scale,
           script::PuzzleGameType type);

/// The maze from its start again: the wolf and the sheep melt away and are made again on their squares
void Reset(entt::entity puzzle);

/// One turn: walls that went or moved are put back; while neither piece walks, the sheep is put back on its square, the
/// first bell rung sends the wolf sliding its way, the wolf turns and then walks to its square, and the corner bell
/// starts the maze again. Every bell is then unrung
void Process(entt::entity puzzle);

/// The test of a solution: the wolf has walked to the sheep's square. A solution sparkles over the sheep and both go,
/// so the test gives no again afterwards, as the original's
bool TestSolution(entt::entity puzzle);

/// A bell has been rung at least once
[[nodiscard]] bool Started(entt::entity puzzle);

} // namespace openblack::ecs::lion_maze_game
