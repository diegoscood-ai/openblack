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

// A villager's age kept as its birth turn, like Living +0xA0 (docs/bw1-notes/villagers.md).
namespace openblack::ecs::villager
{

/// GGameInfo +0xC (0xD01A04): turns in a year, ftol(1500.0f) from 0x8DF8D8 in GGameInfo::GGameInfo 0x557730
inline constexpr uint32_t k_TurnsPerYear = 1500;

/// Living::GetAge 0x5ECAF0: (turn - birthTurn) / 1500, an unsigned division (`xor edx, edx; div`, 0x5ECB01)
[[nodiscard]] constexpr uint32_t AgeFromBirthTurn(int32_t birthTurn, uint32_t turn)
{
	return (turn - static_cast<uint32_t>(birthTurn)) / k_TurnsPerYear;
}

/// Living::SetAge 0x5ED2C0: birthTurn = turn - age * 1500 (`imul`, `sub`, 0x5ED2C5..0x5ED2D6)
[[nodiscard]] constexpr int32_t BirthTurnForAge(uint32_t age, uint32_t turn)
{
	return static_cast<int32_t>(turn - age * k_TurnsPerYear);
}

} // namespace openblack::ecs::villager
