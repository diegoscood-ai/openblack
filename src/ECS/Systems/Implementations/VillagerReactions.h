/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#pragma once

// The Villager handler of ECS/Effects/Reactions: ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for a villager of a
// cell, by reaction type (the ported ones: REACT_TO_FIRE in VillagerFire.cpp, REACT_TO_TELEPORT in
// VillagerTeleport.cpp; the rest reach no villager yet).

// Also the villager's side of a reaction's end that the reaction states share (Villager +0x94 the reaction it follows,
// +0xBC its object: kept by VillagerFire.cpp and VillagerTeleport.cpp for their reaction).

#include <cstdint>

#include <entt/entity/fwd.hpp>

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::villager_reactions
{
/// SetLivingReactionHandler(Villager, ...): at every map load (villager_fire::Clear, villager_teleport::Clear)
void Register();

/// Villager::SetTopState 0x752010 (vt +0x8E8) for the reactions' states: the villager core's. (aproximado) MOVE_TO_POS'
/// exit ExitMoveToPos 0x5EDDA0 is not ported, so the walk a change leaves is ended here (openblack's move tags), unless
/// the exit refused the change (0x2E: nothing changed)
uint32_t SetTopState(entt::entity villager, VillagerStates state);
/// Villager::PopFromPrevious 0x751E50: SetTopState (vt +0x8E8) of the stored state's resume state (Infos +0x30 0xDB9E98,
/// file 0x20); 0x2E -> raw LivingAction::SetState(0, 163) 0x5ECC90; then raw SetState(2, 0)
void PopFromPrevious(entt::entity villager);
/// Villager::ResetStateAfterReacting 0x751E10 (vt +0x9A0): PopFromPrevious, then SetTopState(163) if GetFinalState is
/// a reactive state (Infos +0xC8 0xDB9F30, file 0xB8)
void ResetStateAfterReacting(entt::entity villager);
/// Living::StopReactingAndSetState 0x5F11C0 (vt +0x99C): ResetStateAfterReacting, then StopReacting if still reacting
void StopReactingAndSetState(entt::entity villager);
/// Living +0x94 != 0: it follows a reaction (the fire's or the teleport's)
[[nodiscard]] bool IsReacting(entt::entity villager);
/// Villager::StopReacting 0x7637D0 (vt +0x998) -> Living::StopReacting 0x5F1140: the reaction's record gets the turn,
/// +0x94 = 0, +0xBC = 0
void StopReacting(entt::entity villager);
/// Villager::ExitReaction 0x7527A0 (vt +0x910; the rows hold the thunk 0x5B0100 = jmp [vt +0x910]): the circle hug
/// reset, and StopReacting unless `next` is a reactive state. Always 1 (it may leave)
uint32_t ExitReaction(components::LivingAction& action, VillagerStates next);
} // namespace openblack::ecs::villager_reactions
