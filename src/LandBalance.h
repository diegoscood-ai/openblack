/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

namespace openblack::land_balance
{

/// GLandBalance::Values (0xD1A280): 8 multipliers the land script sets with SET_GLOBAL_LAND_BALANCE, all 1 when a
/// map loads (GLandBalance::Init 0x5E2890, from GSetup::LoadMapFeatures). Known uses: 4 = villager speed
/// (Villager::SetStateSpeed), 5 = the wood value of trees (Tree::GetWoodValue), 7 = the belief speed constant.
constexpr size_t k_Count = 8;

void Reset();
void Set(int index, float value);
[[nodiscard]] float Get(size_t index);
/// [0xBF33F0], the lost-town scale: SET_LOST_TOWN_SCALE (land script case 104, 0x717E85); 1.0 in GLandBalance::Init
/// (0x5E28A9, Reset). Read by the town belief's fold (ecs::town_belief, fn_004383D0 0x4384BA / 0x4386E4)
void SetLostTownScale(float scale);
[[nodiscard]] float LostTownScale();

} // namespace openblack::land_balance
