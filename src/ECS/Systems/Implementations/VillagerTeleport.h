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
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

// The villagers and the teleport stones (VillagerReaction.cpp 0x766200..0x766440): a walking villager that the stone's
// REACT_TO_TELEPORT reaction reaches, and for which another stone of its player is worth the detour, walks to the stone
// (GO_TOWARDS_TELEPORT_REACTION 201 / _QUICKLY 251), jumps (TELEPORT_REACTION 202) and resumes what it was doing.
// Magic/Objects/MagicTeleport does the jump. Wiki: docs/bw1-notes/magic.md, "Teletransporte".

namespace openblack::ecs::villager_teleport
{
/// Object::IsMoving (vt 0x174, 0x402710: the position changed since the last turn). (inf) Here: walking to a goal.
[[nodiscard]] bool IsMoving(entt::entity living);
/// Villager::GetFinalDestPos 0x756AD0 -> Living::GetFinalDestPos 0x5EC1E0 (the footpath's end or the wall hug's goal):
/// here the wall hug's goal while walking, else where it stands. MapCoords as metres (y 0).
[[nodiscard]] glm::vec3 FinalDestination(entt::entity living);
/// Living::GetReaction 0x5ECA60 as far as the stones care: the REACT_TO_TELEPORT reaction it follows, 0 none
[[nodiscard]] uint32_t CurrentReaction(entt::entity living);
/// Villager vt 0x1C GetPlayer: its town's owner
[[nodiscard]] std::optional<PlayerNames> PlayerOf(entt::entity villager);

/// Villager::ReactToTeleportPriority 0x766200: (MagicTeleport::ShouldLivingThingReact ? 0xFF : 0) & the priority of
/// REACT_TO_TELEPORT (the ReactionInfo table in memory, 0xD4FE90 = record 20 +0)
[[nodiscard]] uint8_t ReactToTeleportPriority(entt::entity villager, uint32_t reaction);
/// Villager::SetupReactToTeleport 0x766250: the stone registers its final destination, +0xBC = the stone, and it
/// reacts (AddReaction 0x763440) with GO_TOWARDS_TELEPORT_REACTION
void SetupReactToTeleport(entt::entity villager, entt::entity stone, uint32_t reaction);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t GoToTeleportReaction(components::LivingAction& action); ///< 201 0x7662F0 (251 is a jmp to it: 0x766380)
uint32_t TeleportReaction(components::LivingAction& action);     ///< 202 0x7663F0

/// The REACT_TO_TELEPORT part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for a villager of a cell the reaction
/// reaches (spread once by ECS/Effects/Reactions, when the stone is made: ProcessReactions' respreading flag 0xD00DD4
/// is never set)
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// fn_005FC4F0's villager side before the jump: put down at the stone (fn_005DA0C0), LANDED, DecideWhatToDo
void LandAt(entt::entity villager, const glm::vec3& mapPosition);
/// Villager::DecideWhatToDo (vt 0x8C8)
void DecideWhatToDo(entt::entity villager);
/// After Living::MoveByTeleport's MoveMapObject: a walk in progress goes on from the new position
void OnMoved(entt::entity living);

/// A land is loaded (also registers the villagers' reaction handler)
void Clear();
} // namespace openblack::ecs::villager_teleport
