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

/// fn_006D6680 (from the CHL create switch 0x6F184C): a PuzzleGame of that type at the script's vector (x, altitude +
/// y, z); the angle goes in 2048 steps (ftol(angle x 2048 x 0.159155)). Nothing else happens until it is processed.
entt::entity CreatePuzzleGame(const glm::vec3& position, script::PuzzleGameType type, float yAngleRadians, float scale);

/// fn_006D7480 for one puzzle (GlobalGameLists::Process 0x591449, every turn): nothing once +0x3C is set; if the
/// played test (fn_006D66E0) is true, +0x3C = 1; else the type's step. Type 14 (0x6D7FCD): the first time, the bait,
/// its net and the two shoals (ecs::CreateFishPuzzle); the fish do the rest (ecs::UpdateFishShoals).
void ProcessPuzzleGame(entt::entity puzzle);
/// Every PuzzleGame, and PuzzleGame::ToBeDeleted 0x6D6FF0 (the bait with its FishPlot and the two shoals go) for the
/// ones the scripts deleted
void ProcessPuzzleGamesTurn();

/// The CHL PLAYED of a PuzzleGame (GScript::Played 0x6F9DC0 0x6F9F0B -> fn_006D66E0, switch 0x6D6C94 on type - 1):
/// type 14 (0x6D6A32) is 1 once its bait is done (bait +0x18, set when the 30 fish stayed 500 ms in the net), and
/// that sets +0x3C too; 0 while the bait is not made yet. The other types are not ported: 0.
[[nodiscard]] bool IsPuzzleGamePlayed(entt::entity puzzle);

/// The bait of a fish puzzle (entt::null before its first turn)
[[nodiscard]] entt::entity PuzzleGameBait(entt::entity puzzle);

} // namespace openblack::ecs
