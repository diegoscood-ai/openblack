/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Creature/CreatureDesires.h"
#include "Enums.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

/// A player's hand does something that shows a desire, and the player's creature may see it
namespace openblack::ecs::creature_hand_sight
{

/// The hand shows the desire (gripping a village's totem shows its wish to impress): when the player's creature can see
/// the hand, the player's camera looking at the creature and the creature looking towards the hand, the desire's
/// growing time is divided by the desire's divisor times the species' scale, kept between the species' least and most
/// (creature_hand_sight::IncreaseSecondsAfterSeen). Returns whether the creature saw it
bool SeePlayerAction(Registry& registry, PlayerNames player, creature_desires::Desire desire);

} // namespace openblack::ecs::creature_hand_sight
