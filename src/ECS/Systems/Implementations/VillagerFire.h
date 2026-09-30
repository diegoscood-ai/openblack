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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

// The villagers and fire (VillagerFireman.cpp 0x75A3D0..0x75B460, VillagerReaction.cpp ReactToFire 0x765540..0x765B70):
// REACT_TO_FIRE (215, the REACT_TO_FIRE reaction), PUT_OUT_FIRE_BY_BEATING (216), ON_FIRE (219, burning or fleeing a
// fire) and MOVE_AROUND_FIRE (220). With water (217, 218) they give up at once in W120. Villager +0x114 (the fire it
// deals with) and +0xBC (the object it reacts to) are kept here by entity. Wiki: docs/bw1-notes/magic.md, "Fuego".

namespace openblack::ecs::villager_fire
{
/// Villager +0x114: the fire (FireEffect id) it flees from or fights; 0 none
[[nodiscard]] uint32_t FireOf(entt::entity villager);
/// Villager::IsFireMan 0x75B400: its final state's exit function is ExitPutOutFire (216..218, 220) and nothing else, or
/// it is REACT_TO_FIRE (215). Fighting villagers are not heated by the fire (fn_0072F980).
[[nodiscard]] bool IsFireMan(entt::entity object);
/// GetFinalState (vt 0xB04) == ON_FIRE (0xDB)
[[nodiscard]] bool IsInOnFireState(entt::entity villager);
/// Villager::SetupOnFire 0x75B170: unless in the hand or thrown, it keeps its state and destination, then ON_FIRE with
/// +0x114 = the fire that heats it (0: its own)
void SetupOnFire(entt::entity villager, uint32_t fire);
/// Villager::StopFireFighting 0x75B020 (a fire group dissolves)
void StopFireFighting(entt::entity villager);
/// Villager::SetupReactToFire 0x765540 (the reaction's start function for REACT_TO_FIRE)
void SetupReactToFire(entt::entity villager, entt::entity object, uint32_t reaction);
/// Villager::ReactToFirePriority 0x765610 (the reaction's priority function): 0 = don't
[[nodiscard]] uint8_t ReactToFirePriority(entt::entity villager, uint32_t reaction, uint32_t currentReaction);

// the state table entries (LivingActionSystem.cpp k_VillagerStateTable)
uint32_t ReactToFire(components::LivingAction& action);          ///< 215, Villager::ReactToFire 0x765870
uint32_t PutOutFireByBeating(components::LivingAction& action);  ///< 216, 0x75AC50
uint32_t PutOutFireWithWater(components::LivingAction& action);  ///< 217 0x75AFE0 / 218 0x75B000: DECIDE_WHAT_TO_DO
uint32_t OnFire(components::LivingAction& action);               ///< 219, 0x75B1E0
uint32_t MoveAroundFire(components::LivingAction& action);       ///< 220, 0x75A7E0
bool EnterPutOutFire(components::LivingAction& action, VillagerStates from, VillagerStates to); ///< 0x75ADC0
bool ExitPutOutFire(components::LivingAction& action);           ///< 0x75AE80
bool EnterOnFire(components::LivingAction& action, VillagerStates from, VillagerStates to); ///< 0x75AF30
bool ExitOnFire(components::LivingAction& action);               ///< 0x75AF80

/// The REACT_TO_FIRE part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for a villager of a cell the reaction
/// reaches (ECS/Effects/Reactions spreads it once, when it is made; ProcessReactions' respreading is off: its flag
/// 0xD00DD4 is never set)
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// A land is loaded (also registers the villagers' reaction handler)
void Clear();
} // namespace openblack::ecs::villager_fire
