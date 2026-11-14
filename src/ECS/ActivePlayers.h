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

#include <optional>

#include "Enums.h"

/// The game's walk over its active players: the players of the seven slots before the neutral one that have a type,
/// as the game walks them for the gods' minds and for a shield's anti-influence rings
/// (docs/wiki/magic/shields.md, docs/wiki/hand/computer-minds.md)
namespace openblack::ecs::active_players
{

/// The slots before the neutral one, and all eight
inline constexpr uint8_t k_NotNeutralPlayers = 7;
inline constexpr uint8_t k_AllPlayers = 8;
/// A player's type that is no player
inline constexpr uint32_t k_NoType = 0;
/// The neutral player's type: after a player, the walk stops at a slot of this type
inline constexpr uint32_t k_NeutralType = 3;

/// The next active player after a player (from the first, without one): the first of the seven slots before the
/// neutral one whose type (`typeOf`) is not k_NoType; none past them, or when the slot after `after` is of the
/// neutral's type
template <typename TypeOf>
[[nodiscard]] std::optional<PlayerNames> Next(const TypeOf& typeOf, std::optional<PlayerNames> after)
{
	auto slot = after.has_value() ? static_cast<uint8_t>(static_cast<uint8_t>(*after) + 1) : uint8_t {0};
	// after a player, the walk stops at a neutral one
	if (after.has_value() && slot < k_AllPlayers && typeOf(static_cast<PlayerNames>(slot)) == k_NeutralType)
	{
		return std::nullopt;
	}
	for (; slot < k_NotNeutralPlayers; ++slot)
	{
		if (typeOf(static_cast<PlayerNames>(slot)) != k_NoType)
		{
			return static_cast<PlayerNames>(slot);
		}
	}
	return std::nullopt;
}

} // namespace openblack::ecs::active_players
