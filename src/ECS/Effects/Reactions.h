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

#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"

// Reaction (Reaction.cpp, 0x44 bytes): "something to react to" started by an object (a spell, a crushed villager...).
// Only the data is kept: who started it, its type and player. Spreading it over the map cells to the villagers and the
// creature (SpreadReaction 0x6E3E10, per cell within GetRadius, GameRand for the Living) and their responses belong to
// the villager AI and are not ported.

namespace openblack::ecs::effects::reactions
{
struct Reaction
{
	uint32_t id {0};
	entt::entity initiator {entt::null};
	openblack::Reaction type {openblack::Reaction::None};
	PlayerNames player {PlayerNames::NEUTRAL};
	uint32_t turnCreated {0}; ///< +0x2C, set when created with its last argument 1
	float radius {0.0f};      ///< +0x3C, when its creator sets one (SpellShield: the shield's radius + 30)
};

/// Reaction::CreateReaction 0x6E3D70 (initiator, type, player, stamp): 0 for REACTION -1; else the new reaction's id
uint32_t CreateReaction(entt::entity initiator, openblack::Reaction type, PlayerNames player, bool stamp);

/// Who reacts to a new reaction: CreateReaction 0x6E3D70 spreads it at once over the map cells within its radius
/// (SpreadReaction 0x6E3E10) to the Living that take it. The villagers' side is ported per reaction type (the fire's in
/// ECS/Systems/Implementations/VillagerFire.cpp); a type without a handler reaches nobody.
using SpreadHandler = void (*)(const Reaction& reaction);
void SetSpreadHandler(openblack::Reaction type, SpreadHandler handler);

/// Reaction::RemoveAllReactionsInitiatedByObject 0x6E4750
void RemoveAllReactionsInitiatedByObject(entt::entity initiator);

/// Reaction::RemoveAllReactionsOfTypeInitiatedByObject 0x6E4780 (ShutDown of each)
void RemoveAllReactionsOfTypeInitiatedBy(entt::entity initiator, openblack::Reaction type);

/// fn_006E4830 (FireEffect fn_00730960): the reaction moves to another initiator (a tree's fire to its DeadTree)
void SetInitiator(uint32_t reaction, entt::entity initiator);

/// reaction +0x2C = the game turn (SpellShield::UpdateStruckReaction 0x72B780 refreshes its reaction this way)
void Stamp(uint32_t reaction);
/// reaction +0x3C (SpellShield::InitWithPos 0x72B5F0 writes the radius of its REACT_TO_MAGIC_SHIELD)
void SetRadius(uint32_t reaction, float radius);

/// Reaction::GetReactionInitiatedByObject 0x6E4870: the first one's id, 0 none
[[nodiscard]] uint32_t GetReactionInitiatedBy(entt::entity initiator);

[[nodiscard]] const std::vector<Reaction>& All();
/// The reaction by its id, nullptr when gone
[[nodiscard]] const Reaction* Find(uint32_t id);
/// The game turn of the last SetTurn
[[nodiscard]] uint32_t Turn();

/// The game turn (for the created stamp)
void SetTurn(uint32_t turn);
/// A land is loaded
void Clear();
} // namespace openblack::ecs::effects::reactions
