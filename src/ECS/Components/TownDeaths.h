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

#include <array>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The town's death counters, TownStats (Town +0x610) fields that only Villager::VillagerDead writes (fn_0073E440 ->
/// TownStats fn_00749780) and the TownStats constructor fn_007491F0 zeroes (0x74921C, 0x74922E, 0x749234 and the
/// `rep stosd` 0x7492AE..0x7492CD); TownStats' per-turn recount (ECS/Town/TownStats.cpp) does not touch them, so they live
/// in their own component (ecs::villager::VillagerDead assigns it, zeroed, at a town's first death: the same as zero at
/// the town's creation). Readers found: GetDeaths 0x740D70 (+0x7C[4], GET_TOWN_WORSHIP_DEATHS through
/// Town::GetDeathsFromWorshipping 0x740D60); TownStats-relative readers were not searched (V12 spec Q-7).
struct TownDeaths
{
	uint32_t total38 {0};                 ///< TownStats +0x38 (Town +0x648): ++ (0x7497E5)
	std::array<uint32_t, 8> total5C {};   ///< +0x5C (Town +0x66C): only [0] is written, ++ (0x7497E4)
	std::array<uint32_t, static_cast<size_t>(DeathReason::_COUNT)> byReason {}; ///< +0x7C[reason] ++ (0x7497DA)
	std::array<uint32_t, 9> byPlayer {};  ///< +0xA4[killer's GetPlayerNumber 0x64A790] ++ (0x7497C5; none: the neutral)
	uint32_t lastDeathTurn {0};           ///< +0xD8 = the game turn (0x74978E)
	uint32_t count {0};                   ///< +0xDC ++ (0x749794)
};

} // namespace openblack::ecs::components
