/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Creature/LeashTie.h"
#include "Enums.h"
#include "Input/GamePackets.h"

namespace openblack::ecs::systems
{
class LeashSystemInterface;
} // namespace openblack::ecs::systems

/// Tying the leash with the hand (creature_leash's tie rules). The hand calls the entry points below as the player
/// double clicks or taps; they decide with the rules, send the packet, and the next turn's handlers here carry it out
/// through the leash service. Wiki: docs/bw1-notes/creature.md, "Tying the leash with the hand".
namespace openblack::ecs::leash_tie
{

/// What the tie takes the thing to be: a leash post, a spell icon, a script highlight, a one-off spell seed, or any
/// other thing
[[nodiscard]] creature_leash::TieTargetKind KindOf(entt::entity target);

/// The facts the double click decides on, for the player, the hand's target (entt::null for none) and the leash service
[[nodiscard]] creature_leash::HandTieCheck CheckDoubleClick(const systems::LeashSystemInterface& leash, PlayerNames player,
                                                            entt::entity target);
/// The facts a tap with the leash in the hand decides on
[[nodiscard]] creature_leash::LeashTapCheck CheckTap(const systems::LeashSystemInterface& leash, PlayerNames player);

/// The hand area's entry points. With no leash service, nothing is the leash's.
///
/// A double click of the Action button with the hand on `target`: what is under the hand, or else the nearest thing
/// within 5 of where the hand acts, entt::null for none. Sends the tie or the untie for the next turn and returns the
/// decision. On HandTie::Tap the hand taps the target as a single click would. Whatever the result, the hand then ends
/// its action press and resets its action state, as the original does after every double click.
creature_leash::HandTie OnHandDoubleClick(PlayerNames player, entt::entity target);
/// A single tap on a thing. Before calling it, the hand runs its own start of a tap on the thing (not ported yet),
/// which the original runs first unless the leash is on, untied and the thing is a reward: so it runs when the leash is
/// off, when it is tied, and when the thing is not a reward. Returns true when the tap was the leash's (the creature is
/// sent to act on the thing): the hand then ends its action press and resets its action state, and does not tap.
/// False: the hand taps the thing as it always does.
bool OnHandTap(PlayerNames player, entt::entity target);
/// A single tap on the land at a point. ActOnPoint: the creature is sent to act on the point; the hand then clears its
/// action press, without resetting its action state, and does not tap. Nothing: the point is the origin
/// (creature_leash::IsTapOrigin); nothing is sent and the hand neither taps nor clears its press. NotLeash: the hand
/// taps the land as it always does.
creature_leash::LeashTap OnHandTap(PlayerNames player, const glm::vec3& point);

/// The same as the entry points, with the leash service given
creature_leash::HandTie DoubleClick(const systems::LeashSystemInterface* leash, PlayerNames player, entt::entity target);
bool TapObject(const systems::LeashSystemInterface* leash, PlayerNames player, entt::entity target);
creature_leash::LeashTap TapLand(const systems::LeashSystemInterface* leash, PlayerNames player, const glm::vec3& point);

/// The turn's handlers. Each returns whether it acted.
///
/// LeashTie: the sender's creature must know the learning leash and wear a leash that works. With no object (none
/// sent, or gone since) the leash goes back to the hand; otherwise it is tied to the object, then, whether or not the
/// tie took, the creature is pulled away from what it was doing and told to act on the object.
bool ApplyTie(systems::LeashSystemInterface* leash, const game_packets::Packet& packet);
/// LeashActOnObject: the sender's creature is told to act on the object, unless the player's leash does not work. The
/// leash need not be on: openblack keeps whether the leash works on the worn leash only, so with none on it counts as
/// working, as every leash starts
bool ApplyActOnObject(systems::LeashSystemInterface* leash, const game_packets::Packet& packet);
/// LeashActOnPoint: the sender's creature, wearing a leash that works, would act on the point. Open point: openblack's
/// creature has no way yet to be sent to act on a point of the land, so it does nothing; true when it would have
bool ApplyActOnPoint(const systems::LeashSystemInterface* leash, const game_packets::Packet& packet);

/// The three handlers, through the leash service in the locator
void RegisterPacketHandlers();

} // namespace openblack::ecs::leash_tie
