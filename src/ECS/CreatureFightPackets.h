/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <entt/entity/entity.hpp>

#include "Input/GamePackets.h"

namespace openblack::ecs::systems
{
class CreatureFightSystemInterface;
class ParticleSystemInterface;
} // namespace openblack::ecs::systems

/// The packets the hand sends while it directs the fight of the player's creature, applied at the start of the next
/// turn as every packet of the hand. Each acts on the sending player's creature. Wiki:
/// docs/wiki/creature/fights.md#the-players-presses.
namespace openblack::ecs::creature_fight_packets
{

/// A press on the arena's ground: the player's creature, while it is there and fighting, steps towards the point (the
/// packet's coords), in place of its queued moves or after them. With no creature, one gone or one no longer fighting, a
/// failed command's spot visual shows at the point instead
void ApplyGroundPress(systems::CreatureFightSystemInterface* fights, systems::ParticleSystemInterface* particles,
                      std::optional<entt::entity> playersCreature, const game_packets::Packet& packet, bool replace);

/// A press on a fighter's body: while the thing pressed on (the packet's object) is still there, the move (its value,
/// numbered as the scripts number them) goes into the queue of the player's creature, in place of its queued moves or
/// after them
void ApplyBodyPress(systems::CreatureFightSystemInterface* fights, std::optional<entt::entity> playersCreature,
                    const game_packets::Packet& packet, bool replace);

/// A press on a body let go: the blow it queued takes its charge from how long it was held (the packet's value, in ms)
void ApplyCharge(systems::CreatureFightSystemInterface* fights, std::optional<entt::entity> playersCreature,
                 const game_packets::Packet& packet);

/// A fight miracle's gesture drawn: while the creature (the packet's object) is still there, the miracle (its value, a
/// magic type) goes into its fight queue after its queued moves
void ApplyFightSpell(systems::CreatureFightSystemInterface* fights, const game_packets::Packet& packet);

} // namespace openblack::ecs::creature_fight_packets
