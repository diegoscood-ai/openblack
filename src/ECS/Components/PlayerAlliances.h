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

#include <array>

#include "Enums.h"

namespace openblack::ecs::components
{

/// How strongly a player is allied to each player, by player slot (the neutral one included). Every strength starts at
/// 0; only a script sets them, both ways at once. Above 0 the players are allies; above 0.1 a player's influence also
/// lets an ally act. It lasts the whole game: the player's entity carries it while there is one, and the player
/// system keeps it for a player without one and from one land to the next
struct PlayerAlliances
{
	static constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

	std::array<float, k_Players> strength {};
};

} // namespace openblack::ecs::components
