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

#include <entt/entity/fwd.hpp>

#include "Enums.h"

// A player's resistance to wind, as the scripts set it (SET_PLAYER_WIND_RESISTANCE, GET_PLAYER_WIND_RESISTANCE): with
// its value set, a thing the player's hand throws flies without drag, in the air and in the water.

namespace openblack::ecs::wind_resistance
{
/// The player keeps the value as the script gave it (components::NoWindResistance); a player that is not there,
/// nothing
void Set(PlayerNames player, uint32_t value);
/// The player's value: 0 for a player that is not there or was never given one
[[nodiscard]] uint32_t Get(PlayerNames player);
/// Right after a player's hand has thrown a thing: with the player's value set, the body it has now has no drag
void AfterThrowFromHand(entt::entity object, PlayerNames handPlayer);
} // namespace openblack::ecs::wind_resistance
