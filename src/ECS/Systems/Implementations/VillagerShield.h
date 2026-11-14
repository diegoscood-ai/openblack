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

#include <functional>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
} // namespace openblack::ecs::components

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

// The villagers and the shield spells: a villager the REACT_TO_MAGIC_SHIELD reaction of a SpellShield reaches walks
// under the dome (SetupMoveToWithHug to a point at 0.8 R of its own bearing) and stands there amazed, looking outwards
// (AMAZED_BY_MAGIC_SHIELD_REACTION 168), until the shield or its town's need for it goes. Magic/Spells/SpellShield
// makes the reaction (radius R + 30) when the spell is cast; it is spread then, and again each time the turn's walk
// over the reactions comes to it, to the villagers near it at that moment.
//
// The town gates read the town's desire for protection (GetDesireSignificanceToVillager, through ecs::town_desire) and
// the turn of the town's last aggressor. Both come from the attacks on the town (ecs::town_aggression, the town's
// aggression record): until a town is attacked by someone other than its own player only a script boost puts its
// protection desire above its trigger, so only the homeless react (see ReactToMagicShieldPriority).
// OPENBLACK_TEST_SHIELD_REACTION=1 takes both gates as "the town wants protection and was just attacked" so the
// reaction can be seen in game; without it the gates are the original's reads. Wiki: docs/wiki/magic/shields.md#shields.

namespace openblack::ecs::villager_shield
{
/// The priority of REACTION_REACT_TO_MAGIC_SHIELD (from its ReactionInfo row) when the reaction's initiator is an
/// available SpellShield and either the villager has no town or its town wants protection and was attacked less than
/// numGameTurnsAfterAggressionInterestedInShield turns ago; else 0
[[nodiscard]] uint8_t ReactToMagicShieldPriority(entt::entity villager, uint32_t reaction);

/// Whether the villager's town was attacked lately enough for its villagers to care about a shield, by the villager's
/// own info row; false without a town
[[nodiscard]] bool TownAttackedLately(entt::entity villager);

/// AddReaction(reaction, 168), remembers the spell, and, when the villager is not already under the shield minus 0.2 R
/// (IsUnder), a walk to shield + GetPosFromAngle(the bearing shield -> villager +- pi / 8, 0.8 R (1 - rand^3)) with
/// final state 168. Then, either way, the look-at point 1 m beyond the villager on that same bearing: it faces away
/// from the dome
void SetupReactToMagicShield(entt::entity villager, entt::entity spell, uint32_t reaction);

/// The state function of AMAZED_BY_MAGIC_SHIELD_REACTION (168): turns
/// towards the look-at point, re-aims it once facing (1 in 4), and keeps the state's clip for 20..30 s before rolling
/// another one (IntoPointing goes to TalkingAndPointing after one cycle). With no town, no shield or no need for
/// protection it waits GameRand(60) + 20 turns (WAIT_FOR_COUNTER) and then decides what to do
uint32_t AmazedByMagicShieldReaction(components::LivingAction& action);

/// A bearing turned by a GameFloatRand(pi / 4) draw, less pi / 8, in the original's order: the draw less pi / 8 first,
/// then the bearing added, each step rounded to a float (the shield's walk and look-at point, the panic at a fall)
[[nodiscard]] float JitteredBearing(float bearing, float draw);

/// The REACT_TO_MAGIC_SHIELD part of applying a reaction to the living objects of a cell, for one villager
void ApplyReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// REACT_TO_MAGIC_SHIELD_STRUCK's priority, pure: its row's priority (a byte) for a villager amazed by a shield
/// (its final state 168) whose look-at point is not the struck shield's position (x or z differs), the initiator a
/// shield spell; else 0
[[nodiscard]] uint8_t StruckShieldPriority(bool shieldSpell, bool amazedByShield, glm::ivec2 lookAt, glm::ivec2 shieldPos,
                                           uint8_t priority);
/// The same for a villager and a struck shield's reaction (the initiator a shield spell, whether or not it is still
/// available). (openblack, guard) a villager with no shield state has no look-at point: 0
[[nodiscard]] uint8_t ReactToMagicShieldStruckPriority(entt::entity villager, uint32_t reaction);
/// The struck shield taken up, with no change of state: how impressed it is by the struck reaction, then the struck
/// shield is its object, the reaction it follows is that shield's REACT_TO_MAGIC_SHIELD (none: it follows nothing) and
/// its look-at point is the shield's position. Nothing for an initiator that is not a shield spell
void SetupReactToMagicShieldStruck(entt::entity villager, entt::entity spell, uint32_t reaction);
/// The REACT_TO_MAGIC_SHIELD_STRUCK part of applying a reaction to the living objects of a cell, for one villager that
/// follows no reaction (the spread's first take: the score above 0 and the standard turns before reacting again, the
/// reaction's start turn, then the setup)
void ApplyStruckReaction(entt::entity villager, const effects::reactions::Reaction& reaction);

/// How long before a villager reacts to a shield again: forever when it has a town that was last attacked longer ago
/// than numGameTurnsAfterAggressionInterestedInShield; 0 when the thing is a shield spell its goal is not under; else
/// the standard turns
[[nodiscard]] uint32_t TurnsBeforeReactingToShieldAgain(bool hasTown, bool attackedRecently, bool shield, bool goalUnder,
                                                        uint32_t standard);
/// The same for a villager and the reaction's thing at the spread's distance
[[nodiscard]] uint32_t TurnsBeforeReactingToShieldAgain(entt::entity villager, entt::entity thing, float distance);

/// How long a villager keeps reacting to a shield. With a town, a draw of 4 that is not 0 (three times in four) is
/// followed by a draw of 50; when that draw plus the turns since the town's last attack (unsigned, wrapping) is below
/// numGameTurnsAfterAggressionInterestedInShield it reacts for ever (0x7FFFFFFF), else for the standard turns.
/// Without a town nothing is drawn. `gameRand(n)` draws 0 .. n - 1
[[nodiscard]] uint32_t TurnsToReactToShield(bool hasTown, uint32_t turnsSinceAttack, uint32_t interestedTurns,
                                            uint32_t standard, const std::function<uint32_t(uint32_t)>& gameRand);
/// The same for a villager at `distance` from its shield, with the game's draws, its town and its own villager info
[[nodiscard]] uint32_t TurnsToReactToShield(entt::entity villager, float distance);
/// The reaction's own part of a villager's turn while it follows a shield reaction, before its state: the reaction
/// gone -> StopReacting; the spell gone -> StopReactingAndSetState; reacting for more turns than TurnsToReactToShield
/// (signed) -> StopReactingAndSetState. Nothing without a shield reaction
void ProcessReaction(entt::entity villager);

/// It follows a shield reaction
[[nodiscard]] bool IsReacting(entt::entity villager);
/// The shield spell of the reaction it follows (entt::null: none)
[[nodiscard]] entt::entity ReactionObject(entt::entity villager);
/// ReactionValidate's availability test for the reaction's shield spell: still a live SpellShield
[[nodiscard]] bool IsReactionObjectAvailable(entt::entity villager);
/// Whether a shield spell is still a live SpellShield (the same test)
[[nodiscard]] bool IsShieldSpellAvailable(entt::entity spell);
/// StopReacting for the shield reaction: its record gets the turn, and the reaction and its spell are cleared
void StopReacting(entt::entity villager);
/// A land is loaded
void Clear();
} // namespace openblack::ecs::villager_shield
