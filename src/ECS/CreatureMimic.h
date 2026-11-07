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

#include <entt/entity/fwd.hpp>

#include "Creature/CreatureDeeds.h"
#include "Enums.h"

// What the game tells the players' creatures about their players: the deeds a creature may copy, and the town needs
// it may come to share. Each is published as an event (ECS/Events/CreatureMimicEvents.h) and only read through the
// const registry, so with no creature in the world nothing changes. Docs:
// docs/wiki/creature/mimicry.md#mimicry-and-the-town-hooks.

namespace openblack
{
class EventManager;
} // namespace openblack

namespace openblack::ecs::creature_mimic
{
/// A player did a deed to an object: the player's creature may copy it if it saw it. The point is the object's position
/// (the origin for an object with none).
void Consider(PlayerNames player, creature_watching::Deed deed, entt::entity object,
              std::optional<MagicType> magic = std::nullopt);

/// A thing that a player's hand dropped has come to rest: near the player's temple it is taken home, or stolen and put
/// by the temple when it is another player's town's; away from it, another player's town's thing put by the player's
/// own nearest town is a theft put in the town
void ConsiderLanding(entt::entity object, PlayerNames player);

/// A thing came to rest or left a hand: ConsiderLanding for the player whose interface let go of it last
/// (last_dropped::WhoLastDropped); nothing when no hand did
void ConsiderObjectLands(entt::entity object);

/// A villager or an animal has sunk while it could still be reached: the player whose interface let go of it last
/// (last_dropped::WhoLastDropped) threw it in the sea; nothing when no hand did
void ConsiderThrownInTheSea(entt::entity thing);

/// Whether a hit on a building counts as the player's deed of damaging it by throwing: the hit has a player, and the
/// building's own body is marked as thrown from the hand
[[nodiscard]] bool ShouldMimicBuildingHit(std::optional<PlayerNames> player, uint32_t buildingBodyFlags);

/// A villager of a player saw to one of the town's needs, with a weight: the player's creature may come to share the
/// need. Nothing without a player.
void EmpathiseWithTownDesire(std::optional<PlayerNames> player, TownDesireInfo desire, float weight, entt::entity villager);

/// The handlers that pass the player's deeds, and the town needs the player's villagers see to, on to the creatures'
/// minds, when there are minds to pass them to
void AddMimicEventHandlers(EventManager& manager);
} // namespace openblack::ecs::creature_mimic
