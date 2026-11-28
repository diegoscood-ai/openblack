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

#include <entt/entity/entity.hpp>

#include "Creature/CreatureMarks.h"
#include "ECS/CreatureSkin.h"
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

/// The packets the hand sends about a creature it held, applied at the start of the next turn as every packet of
/// the hand. Wiki: docs/wiki/hand/states.md#the-hand-on-a-creature.
namespace openblack::ecs::creature_hand_packets
{

/// How the hand treated a creature (the packet's data[0], from -1 to 1) reaches its mind, if the creature is still there
void ApplyFeedback(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet);

/// The bruise a hard slap leaves: where on the slapped creature's mesh the hand met it this frame (none when the body
/// was met by its capsules, with no mesh loaded), and its column in the damage marks (creature_feedback::BruiseColumn)
struct Bruise
{
	std::optional<creature_skin::MeshSpot> spot;
	uint8_t column {0};

	bool operator==(const Bruise&) const = default;
};
/// The hard slap's packet, carrying its bruise
[[nodiscard]] game_packets::Packet HardSlapPacket(const Bruise& bruise);
/// The bruise a hard slap's packet carries
[[nodiscard]] Bruise BruiseOf(const game_packets::Packet& packet);
/// The mark a bruise leaves where it lands on the skin: of the first kind, fresh, in the bruise's column
[[nodiscard]] creature_marks::Mark BruiseMark(const creature_skin::SkinPlace& place, uint8_t column);

/// A hard slap bruised: the sending player's own creature (whichever creature was slapped) loses a hundredth of its
/// life, never going below a hundredth, then takes the bruise on its skin where the spot the slap met is on its own
/// mesh (creature_skin::PlaceOf), when the packet carries one
void ApplyHardSlap(std::optional<entt::entity> playersCreature, const game_packets::Packet& packet);

/// The hand gives what it holds to a creature, if both are still there: the creature turns to the camera and reaches
/// for it. Whether the creature took the giving.
bool ApplyGive(systems::CreatureMindSystemInterface* minds, const game_packets::Packet& packet);
/// The hand went too far from the creature it gave to: the creature gives up what it does, as a failure, unless it is at
/// its take with something in its hand; reaching for the thing, its hand plays on to the moment of taking and closes on
/// nothing there
void ApplyGiveCallOff(systems::CreatureMindSystemInterface* minds, systems::CreatureObjectActionSystemInterface* hands,
                      const game_packets::Packet& packet);

/// The player clicked their own creature, or the square spiral put the leash on: no leash-on waits for the leash picker
/// any more, and then the same as their leash key
void ApplyLeashClick(systems::LeashSystemInterface* leash, PlayerNames player, magic::gestures::LeashSelection* picker);

} // namespace openblack::ecs::creature_hand_packets
