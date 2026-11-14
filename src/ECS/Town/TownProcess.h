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

namespace openblack
{
enum class PlayerNames : uint8_t;
}

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
/// Before the players' turns (only the OPENBLACK_TEST_TOWN_DESIRE debug boost)
void StartPlayersTurn();
/// In the player's own turn, first: its citadel
void ProcessPlayerCitadel(PlayerNames player);
/// In the player's own turn, after its citadel: each of its towns in order (map_cells::TownsOf), and its people counted
/// again as they go. A town taken over in its turn ends the walk
void ProcessPlayerTowns(PlayerNames player);
/// At the end of the player's own turn, every ten thousand turns: another player's hand where its interface reports it,
/// near the local player's own, may draw the advisors' rare remark that another god is watching
void ProcessPlayerWatching(PlayerNames player);
} // namespace openblack::ecs::town_process
