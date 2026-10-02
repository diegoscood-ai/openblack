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

// The villagers and the shield spells (VillagerReaction.cpp 0x765BB0..0x766010): a villager the REACT_TO_MAGIC_SHIELD
// reaction of a SpellShield reaches walks under the dome (Living::SetupMoveToWithHug to a point at 0.8 R of its own
// bearing) and stands there amazed, looking outwards (AMAZED_BY_MAGIC_SHIELD_REACTION 168), until the shield or its
// town's need for it goes. Magic/Spells/SpellShield makes the reaction (radius R + 30) when the spell is cast; it is
// spread once, so only the villagers near it at that moment get the chance to react.
//
// Two of the original's inputs have no openblack equivalent yet and keep the villagers from ever reacting unless they
// are homeless (see ReactToMagicShieldPriority):
//  - TownDesire::GetDesireSignificanceToVillager(town +0x34, TOWN_DESIRE_FOR_PROTECTION 3) 0x746660: the three desire
//    arrays it adds (TownDesire +0x90 / +0xD4 / +0x118) are not ported (components::TownDesire only keeps what
//    Villager::AdjustTownModifier writes);
//  - Town::UpdateAggressor 0x73C9B0 only has its two record fields ported (components::Town::aggressor*), written by
//    the physical shield's impacts; nothing else makes a town's aggressor yet.
// OPENBLACK_TEST_SHIELD_REACTION=1 takes both as "the town wants protection and was just attacked" so the reaction can
// be seen in game; without it nothing changes for a villager with a town. Wiki: docs/bw1-notes/miracles.md, "Escudos".

namespace openblack::ecs::villager_shield
{
/// Villager::ReactToMagicShieldPriority 0x765BB0 (vt +0x8B0 of the REACT_TO_MAGIC_SHIELD row): the priority of
/// REACTION_REACT_TO_MAGIC_SHIELD (ReactionInfo[13] +0x10, 0xD4FBD4) when the reaction's initiator is an available
/// SpellShield and either the villager has no town (0x765C08) or its town wants protection and was attacked less than
/// numGameTurnsAfterAggressionInterestedInShield turns ago (0x765C21..0x765C46); else 0
[[nodiscard]] uint8_t ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction);

/// Villager::SetupReactToMagicShield 0x765C60: AddReaction(reaction, 168) (vt +0x990), +0xBC = the spell, and, when the
/// villager is not already under the shield minus 0.2 R (SpellShield::IsUnder 0x72BD20), a walk to
/// shield + GetPosFromAngle(the bearing shield -> villager +- pi / 8, 0.8 R (1 - rand^3)) with final state 168. Then,
/// either way, the look-at point (+0x10C) 1 m beyond the villager on that same bearing: it faces away from the dome
void SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction);

/// Villager::AmazedByMagicShieldReaction 0x765E00, the state function of AMAZED_BY_MAGIC_SHIELD_REACTION (168): turns
/// towards the look-at point, re-aims it once facing (1 in 4), and keeps the state's clip for 20..30 s before rolling
/// another one (IntoPointing goes to TalkingAndPointing after one cycle). With no town, no shield or no need for
/// protection it waits GameRand(60) + 20 turns (WAIT_FOR_COUNTER) and then decides what to do
uint32_t AmazedByMagicShieldReaction(components::LivingAction& action);

/// The REACT_TO_MAGIC_SHIELD part of ApplyReactionToLivingObjectsAtSquare 0x6E3F90 for one villager of the cell
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// Living +0x94 != 0: it follows a shield reaction
[[nodiscard]] bool IsReacting(entt::entity villager);
/// Living +0xBC: the shield spell of the reaction it follows (entt::null: none)
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// Villager::ReactionValidate's IsAvailable (vt 0x2C) for +0xBC when it is a shield spell: still a live SpellShield
[[nodiscard]] bool IsReactionObjectAvailable(entt::entity villager);
/// Living::StopReacting 0x5F1140 for the shield reaction: its record gets the turn, +0x94 = 0, +0xBC = 0
void StopReacting(entt::entity villager);
/// A land is loaded
void Clear();
} // namespace openblack::ecs::villager_shield
