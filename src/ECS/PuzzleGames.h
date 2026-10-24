/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs
{

/// A PuzzleGame of that type at the script's vector (x, altitude + y, z); the angle goes in 2048 steps
/// ((int)(angle x 2048 / 2 pi)). Nothing else happens until it is processed.
entt::entity CreatePuzzleGame(const glm::vec3& position, script::PuzzleGameType type, float yAngleRadians, float scale);

/// One puzzle's turn (every turn): nothing once played; if the played test is true, it is played; else the type's
/// step. Type 14: the first time, the bait, its net and the two shoals (ecs::CreateFishPuzzle); the fish do the rest
/// (ecs::UpdateFishShoals).
void ProcessPuzzleGame(entt::entity puzzle);
/// Every PuzzleGame, and the parts of the ones the scripts deleted go (the bait with its fish plot and the two
/// shoals)
void ProcessPuzzleGamesTurn();

/// The puzzle types that have nothing to play: complete from the start. The two mazes and the second fish puzzle, and
/// a type that is no puzzle at all (none, or past the last)
[[nodiscard]] constexpr bool CompletesAtOnce(script::PuzzleGameType type)
{
	switch (type)
	{
	case script::PuzzleGameType::Maze1:
	case script::PuzzleGameType::Maze2:
	case script::PuzzleGameType::Fishes2:
	case script::PuzzleGameType::None:
		return true;
	default:
		return type >= script::PuzzleGameType::Last;
	}
}

/// The CHL PLAYED of a PuzzleGame, by type: type 14 is 1 once its bait is done (set when the 30 fish stayed 500 ms in
/// the net), and that marks it played too; 0 while the bait is not made yet. A type that CompletesAtOnce is 1, and
/// marked played, from the start. The other types are not ported: 0.
[[nodiscard]] bool IsPuzzleGamePlayed(entt::entity puzzle);

/// The bait of a fish puzzle (entt::null before its first turn)
[[nodiscard]] entt::entity PuzzleGameBait(entt::entity puzzle);

/// A puzzle game's status for the scripts, from whether it is complete and whether its play has begun: until it is
/// complete, in progress once begun, else not yet begun; once complete, won. (pending) a complete immersion puzzle
/// gives the result it recorded, and the lion and sheep puzzle won the good or the evil way; neither is ported, so
/// neither is ever complete here
[[nodiscard]] constexpr script::PuzzleGameStatus PuzzleGameStatusFrom(bool complete, bool begun)
{
	if (!complete)
	{
		return begun ? script::PuzzleGameStatus::GameInProgress : script::PuzzleGameStatus::GameNotYetBegun;
	}
	return script::PuzzleGameStatus::Won;
}

/// The CHL GET_OBJECT_STATE of a PuzzleGame: complete is the played test (IsPuzzleGamePlayed, which marks it played);
/// a fish puzzle's play has begun once its bait is made. The other types are not ported: never begun
[[nodiscard]] script::PuzzleGameStatus PuzzleGameStatusOf(entt::entity puzzle);

} // namespace openblack::ecs
