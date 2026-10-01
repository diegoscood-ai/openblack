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

#include <glm/vec3.hpp>

// The spell presence grid (u8[64][64] at 0xD9C370, 80 m cells): where spells were this turn. Its readers are
// UNVERIFIED (the minimap or the computer player, inf).

namespace openblack::magic::spell_grid
{
/// fn_00721570: the cell of a map position (x, z metres) takes that value
void Mark(const glm::vec3& position, uint8_t value);
/// fn_007215C0, every turn: every cell loses k (to 0 when it is k or less) and k goes back to 0x20. The original does
/// it only when g_game +0x205A28 == 1, else k grows by 0x20 up to 0x100 (UNVERIFIED flag; taken as 1).
void Decay();
[[nodiscard]] uint8_t At(const glm::vec3& position);
void Clear();
} // namespace openblack::magic::spell_grid
