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

// Town::Process 0x747380 of runblack.exe W120 and the players' loop that calls it (GPlayer::ProcessPlayers 0x649A20
// -> GPlayer::Process 0x6494E0 -> Town::Process 0x649551; spec dev\tmp_dis\aldeanos\V3_spec.md §7). The steps of other
// milestones and sessions are TODO with their address, or calls to the sessions' existing functions.

namespace openblack::ecs::town_process
{
/// Town::Process 0x747380, its steps in order (V3: the stats, +0x5E4, TownDesire::Process, the worship every 10
/// turns, the pulse +0x5E8 / +0x5EC and the empty town countdown +0xF20; the rest TODO)
void ProcessTown(entt::entity town);
/// GPlayer::ProcessPlayers 0x649A20's town part: for every player and then the neutral one (GetNextPlayerAndNeutral
/// 0x550980), each of its towns (GPlayer +0xA50) in order (map_cells::ForEachTown). Game::GameLogicLoop calls it
/// between the sharks (0x54E5C7) and GlobalGameLists (0x54E651), before the villagers (Living::ProcessLiving 0x54E65B)
void ProcessPlayers();
} // namespace openblack::ecs::town_process
