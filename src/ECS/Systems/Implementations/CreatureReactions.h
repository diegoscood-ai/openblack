/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The Creature handler of ECS/Effects/Reactions: a creature in a cell a reaction spreads over takes it up as a villager
// or an animal does (its availability, the score, its records, the rule to switch from the reaction it follows), and
// follows it until the reaction ends, what started it goes, or its time is up. Only the miracles' reactions are taken
// up so far: a nasty miracle (FLEE_FROM_SPELL), a shield (REACT_TO_MAGIC_SHIELD) and a nice or an impressive miracle
// (LOOK_AT_NICE_SPELL, REACT_TO_IMPRESSIVE_SPELL); the creature scores every other type 0. The rules themselves are
// pure (Creature/CreatureReactionRules.h); what the creature then does is its mind's (ReactToNastyMagic /
// ReactToNiceMagic). Wiki: docs/bw1-notes/creature.md, "Reactions to miracles".

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

#include <cstdint>

#include <entt/entity/fwd.hpp>

namespace openblack::ecs::effects::reactions
{
struct Reaction;
} // namespace openblack::ecs::effects::reactions

namespace openblack::ecs::creature_reactions
{
/// SetLivingReactionHandler(Creature, ...) and SetLivingShutDownHandler(Creature, ...), once the game's reactions
/// exist (before any map load)
void RegisterHandlers();
/// The handler: applies a reaction to one creature of a cell at the spread's distance. Public for the tests
void HandleReaction(entt::entity creature, const effects::reactions::Reaction& reaction, float distance);
/// Once a creature turn, before its mind thinks: the reaction it follows ends when the reaction or what started it has
/// gone, or once its turns to react are over
void ProcessReaction(entt::entity creature);
/// It stops following its reaction: the reaction's record gets the turn, and it follows nothing
void StopReacting(entt::entity creature);
/// The reaction it follows, 0 none
[[nodiscard]] uint32_t ReactionOf(entt::entity creature);
} // namespace openblack::ecs::creature_reactions
