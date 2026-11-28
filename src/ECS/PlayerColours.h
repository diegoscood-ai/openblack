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
#include <cstdint>

#include <array>

#include "Enums.h"

// The colour each player is drawn in. The players' colours are one table, but a land can give its second and third
// players the colours of others: the colour index. The influence borders keep the colour index as their player, and
// so does a temple's model, whose finished build shows that index's border; a village centre's column of light tests
// the border of its owner's own number instead.

namespace openblack::ecs::player_colours
{

/// The players' colours by colour index, ARGB; the neutral player's is black
inline constexpr std::array<uint32_t, 8> k_Colours = {0xFFFF4646u, 0xFF47FF54u, 0xFFE347FFu, 0xFF47F9FFu,
                                                      0xFFFFFD47u, 0xFF4777FFu, 0xFFFFA247u, 0xFF000000u};

/// The colour index of a player on a land: lands 1, 4 and 5 give the second player the seventh's, land 2 gives the
/// second and third the fifth's and sixth's, and land 3 gives the second the sixth's. Every other player, and every
/// player on any other land, keeps its own number
[[nodiscard]] constexpr PlayerNames Remapped(PlayerNames player, int32_t landNumber)
{
	switch (landNumber)
	{
	case 1:
	case 4:
	case 5:
		return player == PlayerNames::PLAYER_TWO ? PlayerNames::PLAYER_SEVEN : player;
	case 2:
		if (player == PlayerNames::PLAYER_TWO)
		{
			return PlayerNames::PLAYER_FIVE;
		}
		return player == PlayerNames::PLAYER_THREE ? PlayerNames::PLAYER_SIX : player;
	case 3:
		return player == PlayerNames::PLAYER_TWO ? PlayerNames::PLAYER_SIX : player;
	default:
		return player;
	}
}

/// A player's colour on a land, ARGB, from its colour index
[[nodiscard]] constexpr uint32_t Colour(PlayerNames player, int32_t landNumber)
{
	return k_Colours.at(static_cast<size_t>(Remapped(player, landNumber)) & 7u);
}

/// The land number the land's script set (SET_LAND_NUMBER), 0 before any script or without the map script service
[[nodiscard]] int32_t LandNumber();

/// A player's colour index on the current land
[[nodiscard]] PlayerNames OnThisLand(PlayerNames player);

} // namespace openblack::ecs::player_colours
