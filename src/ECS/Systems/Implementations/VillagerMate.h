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

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager held in a player's hand calls to the villagers of the other sex: a villager of breeding age picked up by
/// its own player's hand makes the villager-in-hand reaction, and those of the other sex and the same player, of
/// breeding age too, stop and look at it while it is near. The state function is the living action system's row 235.
/// Wiki: docs/wiki/beings/villager-reactions.md#a-held-villager-reaction-32.
namespace openblack::ecs::villager_mate
{

/// A villager is taken into a hand of `player` (none: a hand of no player, which calls nobody): when the villager is
/// that player's own and of breeding age, the villager-in-hand reaction, made with that player
void HeldByHand(entt::entity villager, std::optional<PlayerNames> player);

/// The take-up (the slot type's setup): the reaction and its state (235), then the held villager as its object
void SetupReactToVillagerInHand(entt::entity villager, entt::entity held, uint32_t reaction);
/// The breeder's call's priority for a villager of breeding age, of the other sex and the same player as the held one,
/// which is still in the hand, and not held by a script; 0 otherwise
[[nodiscard]] uint8_t ReactToVillagerInHandPriority(entt::entity villager, uint32_t reaction, uint32_t other);

/// 235 WAIT_FOR_MATE: within the reaction's reach of the held villager it turns to it and, once facing it and ready
/// for a new clip, plays its state's clip; out of reach it stops reacting and goes back to what it was doing. Returns 1
uint32_t WaitForMate(components::LivingAction& action);

} // namespace openblack::ecs::villager_mate
