/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Enums.h"
#include "Input/GamePackets.h"

namespace openblack::ecs::systems
{
class CreatureMindSystemInterface;
class CreatureObjectActionSystemInterface;
class LeashSystemInterface;
} // namespace openblack::ecs::systems

namespace openblack::magic::gestures
{
struct LeashSelection;
} // namespace openblack::magic::gestures

/// The two packets the hand sends about a creature it held, applied at the start of the next turn as every packet of
/// the hand. Wiki: docs/wiki/hand/states.md#the-hand-on-a-creature.
namespace openblack::ecs::creature_hand_packets
{

/// How the hand treated a creature (the packet's data[0], from -1 to 1) reaches its mind, if the creature is still there
void ApplyFeedback(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet);

/// The hand gives what it holds to a creature, if both are still there: the creature turns to the camera and reaches
/// for it. Whether the creature took the giving.
bool ApplyGive(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet);
/// The hand went too far while the creature reached for what it gave it: the creature gives up taking it, as a
/// failure, its hand closing on nothing if it was already reaching
void ApplyGiveCallOff(systems::CreatureMindSystemInterface* minds, systems::CreatureObjectActionSystemInterface* hands,
                      const game_packets::Packet& packet);

/// The player clicked their own creature, or the square spiral put the leash on: no leash-on waits for the leash picker
/// any more, and then the same as their leash key
void ApplyLeashClick(systems::LeashSystemInterface* leash, PlayerNames player, magic::gestures::LeashSelection* picker);

} // namespace openblack::ecs::creature_hand_packets
