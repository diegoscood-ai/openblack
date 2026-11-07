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

// The town's turn and the players' loop that calls it (each player, then each of its towns). The steps not ported yet
// are TODO, or calls to existing functions of other systems.

namespace openblack::ecs::town_process
{
/// The empty town countdown after a town's turn, and whether the town empties at it
struct EmptyCountdownStep
{
	uint32_t countdown {0};
	bool empties {false};
};

/// A town's empty countdown for its turn: a running countdown goes down by one and the town empties as it reaches 0;
/// short of 0, a town with people again stops it. A countdown at 0 stays there
[[nodiscard]] EmptyCountdownStep StepEmptyCountdown(uint32_t countdown, uint32_t people);

/// The town's turn, its steps in order (the stats, the plan request flag, the desires, the worship every 10 turns, the
/// build pulse and the empty town countdown; the rest TODO)
void ProcessTown(entt::entity town);
/// The players' town part: for every player and then the neutral one, each of its towns in order
/// (map_cells::ForEachTown). The game logic loop calls it between the sharks and the global game lists, before the
/// villagers
void ProcessPlayers();
} // namespace openblack::ecs::town_process
