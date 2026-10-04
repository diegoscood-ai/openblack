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

#include "ECS/Components/LivingAction.h"
#include "Enums.h"

namespace openblack::ecs::effects::reactions
{
struct Reaction;
}

// The neighbours' mourning of a dead villager and its orphans, as runblack.exe W120 does it (VillagerReaction.cpp /
// VillagerStates.cpp; V12 spec §8, dump dev\documentacion\aldeanos\v12\mourn.txt): REACT_TO_DEATH (23), created by
// Villager::Dying 0x76A5C5, spreads once over 60 m; ReactToDeathPriority 0x766440 and SetupReactToDeath 0x7665B0 start the
// villagers' states 205 POINT_AT_DEAD_PERSON, 206 GO_TOWARDS_DEAD_PERSON, 207 LOOK_AT_DEAD_PERSON and 208
// MOURN_DEAD_PERSON. A dead mother's children go to 131 MORN_DEATH (FindChildrenAndOrphanThem 0x756BE0). Villager +0x94
// (the reaction followed) and +0xBC (the dead villager) are kept here, as VillagerFire / VillagerShield keep theirs.

namespace openblack::ecs::villager_mourning
{
// ---- the pure layer ----------------------------------------------------------------------------------------------

/// PointAtDeadPerson 0x76669C..0x7666D6: ftol((GameFloatRand(20) + 2) x (1000 / msPerTurn [0xD01A38] = 100, an unsigned
/// division)) facing turns
[[nodiscard]] int32_t PointTurns(float roll20);
/// MournDeadPerson 0x766854..0x766894: ftol((GameFloatRand(3) + 4) x (1000 / 100)) turns
[[nodiscard]] int32_t MournTurns(float roll3);

// ---- the reaction ------------------------------------------------------------------------------------------------

/// Villager::ReactToDeathPriority 0x766440: a creature initiator -> REACT_TO_DEATH's priority (byte 0xD4FFBC); else my town
/// has a functional graveyard (+0x748, vt +0xD4), I am the dead one, or 10 Livings already took it (reaction +0x1C) -> 0;
/// else the priority
[[nodiscard]] uint8_t ReactToDeathPriority(entt::entity villager, uint32_t reaction);
/// Villager::SetupReactToDeath 0x7665B0: a creature initiator -> AddReaction(r, 7 LOOKING_AT_OBJECT_REACTION); else
/// GameRand(2) (VillagerReaction.cpp 0x726): 0 -> counter (+0x58) = 0, AddReaction(r, 205); 1 -> AddReaction(r, 206);
/// +0xBC = the dead villager
void SetupReactToDeath(entt::entity villager, entt::entity dead, uint32_t reaction);
/// The REACT_TO_DEATH part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell (the fire's,
/// teleport's and shield's rules: available, not reacting, within maxReactionDistance, fn_006E4620's score, not reacted
/// lately), then StartReacting (vt +0x994) -> SetupReactToDeath
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);
/// Living +0x94: it follows a REACT_TO_DEATH
[[nodiscard]] bool IsReacting(entt::entity villager);
/// +0xBC: the dead villager it mourns
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// Living::StopReacting 0x5F1140 for the REACT_TO_DEATH kept here: its record gets the turn, +0x94 = 0, +0xBC = 0
void StopReacting(entt::entity villager);

// ---- the states --------------------------------------------------------------------------------------------------

/// 205 POINT_AT_DEAD_PERSON 0x766680
uint32_t PointAtDeadPerson(components::LivingAction& action);
/// 206 GO_TOWARDS_DEAD_PERSON 0x766700
uint32_t GoTowardsDeadPerson(components::LivingAction& action);
/// 207 LOOK_AT_DEAD_PERSON 0x766810
uint32_t LookAtDeadPerson(components::LivingAction& action);
/// 208 MOURN_DEAD_PERSON 0x766850
uint32_t MournDeadPerson(components::LivingAction& action);
/// The exit of 205-208: the thunk 0x5B0100 = Villager::ExitReaction 0x7527A0 (villager_reactions::ExitReaction), which
/// stops this reaction too unless `next` is a reactive state. Always 1
uint32_t ExitReaction(components::LivingAction& action, VillagerStates next);
/// The validate of 205-208: Villager::ReactionValidate 0x756A00 for REACT_TO_DEATH: the dead villager gone or not
/// available (Villager::IsAvailable 0x751D50), or in the hand (row 23 finishesIfInitiatorInHand 1) -> PopFromPrevious
bool ReactionValidate(components::LivingAction& action);

// ---- orphans -----------------------------------------------------------------------------------------------------

/// Villager::FindChildrenAndOrphanThem 0x756BE0: every villager of the town's structures (+0x754, their inhabitants) and
/// of its homeless list (+0x768) whose mother (+0x100) is `mother` -> MakeChildOrphaned 0x7580D0
void FindChildrenAndOrphanThem(entt::entity mother);
/// Villager::MakeChildOrphaned 0x7580D0 (mother): the child's mother (+0x100) is not her -> 0; IsVillagerAvailable
/// 0x752290 -> SetTopState(131 MORN_DEATH); mother = 0; 1
uint32_t MakeChildOrphaned(entt::entity child, entt::entity mother);

/// A new map: no mourners
void Clear();
} // namespace openblack::ecs::villager_mourning
