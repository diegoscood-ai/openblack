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

// A tree's angle as the original keeps it, and the wind slot it sways in (docs/wiki/world/trees.md#swaying)
namespace openblack::ecs::tree_angles
{
/// A turn's fraction per radian and the radians of a turn, as the game's single floats
inline constexpr float k_TurnsPerRadian = 0.159154937f;
inline constexpr float k_RadiansPerTurn = 6.28318548f;
/// The wind slots per radian (16 slots a turn)
inline constexpr float k_SlotsPerRadian = 2.54647899f;

/// The angle a new tree keeps: the angle it is made with less its whole turns, counted towards zero, so that it stays
/// within a turn on its own side of zero
[[nodiscard]] constexpr float StoredYAngle(float yAngle)
{
	const float turns = static_cast<float>(static_cast<int32_t>(yAngle * k_TurnsPerRadian));
	return yAngle - turns * k_RadiansPerTurn;
}

/// Which of the 16 wind slots a tree of this kept angle sways in: the angle in slots, rounded by adding a half and
/// cutting towards zero, its low four bits
[[nodiscard]] constexpr uint8_t WindSlotOf(float storedYAngle)
{
	return static_cast<uint8_t>(static_cast<int32_t>(storedYAngle * k_SlotsPerRadian + 0.5f) & 0xF);
}
} // namespace openblack::ecs::tree_angles
