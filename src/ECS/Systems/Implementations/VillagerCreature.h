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
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::ecs::components
{
struct LivingAction;
}

/// A villager near a creature (the creature's REACT_TO_CREATURE reaction): by its town's attitude to the creature it
/// inspects it, walking closer after each look, stands by it in respect, or runs from it in fear; a town that does not
/// care makes it stop. Every turn of the reaction the town's attitude may move it from one to another. The rules are
/// ecs::effects::creature_reaction (ECS/Effects/VillagerCreatureRules.h); the state functions are the living action
/// system's rows 146-148, 155-157, 159, 160 and 167. Wiki: docs/wiki/beings/villager-reactions.md#a-creature-reaction-6.
namespace openblack::ecs::villager_creature
{

/// The take-up (the slot type's setup): nothing for a creature that is not available. With no town it inspects it;
/// else its town keeps a record of the creature and the villager inspects it or, for a creature the town fears, runs
/// from it; for one the town does not care about it takes nothing up, the creature only becoming the object of what it
/// does. The creature is its object before its state changes, so the new state's clip sees it
void SetupReactToCreature(entt::entity villager, entt::entity creature, uint32_t reaction);
/// effects::creature_reaction::VillagerPriority on the villager and the reaction's creature
[[nodiscard]] uint8_t ReactToCreaturePriority(entt::entity villager, uint32_t reaction, uint32_t other);
/// effects::creature_reaction::TurnsToReact on the creature's last action and the villager's town
[[nodiscard]] uint32_t TurnsToReactToCreature(entt::entity villager, entt::entity object, uint8_t type, float distance);
/// effects::creature_reaction::TurnsBeforeReactingAgain on the creature's last and current actions and the villager's
/// town
[[nodiscard]] uint32_t TurnsBeforeReactingAgainToCreature(entt::entity villager, entt::entity object, uint8_t type,
                                                          float distance);

/// Every state below starts the same way: the creature gone or not available, it stops reacting and goes back to what
/// it was doing. Each returns 1.
/// 146 INSPECT_CREATURE_REACTION: it starts a look from 20 metres, of one and a half seconds plus a draw below three
uint32_t InspectCreatureReaction(components::LivingAction& action);
/// 147 PERFORM_INSPECT_CREATURE_REACTION: a turn of its look, turning to the creature; at the look's end it walks
/// closer, or near enough it stops reacting
uint32_t PerformInspectCreatureReaction(components::LivingAction& action);
/// 148 APPROACH_CREATURE_REACTION: it walks up to the creature when further than it looks from, else it looks again
/// from 8 metres closer, for one and a half seconds plus a draw below seven
uint32_t ApproachCreatureReaction(components::LivingAction& action);
/// 155 INITIALISE_RESPECT_CREATURE_REACTION: it walks to the creature's edge nearest to it
uint32_t InitialiseRespectCreatureReaction(components::LivingAction& action);
/// 156 PERFORM_RESPECT_CREATURE_REACTION: facing away it turns back (160); else it counts four seconds of facing it
uint32_t PerformRespectCreatureReaction(components::LivingAction& action);
/// 157 FINISH_RESPECT_CREATURE_REACTION: it stops reacting, the creature there or not
uint32_t FinishRespectCreatureReaction(components::LivingAction& action);
/// 159 FLEEING_FROM_CREATURE_REACTION: beyond one and a half times its run it gives up; else, while the creature acts on
/// something, one draw in ten runs it the run's length straight away from the creature at half its fleeing speed; with
/// the creature acting on nothing it stands at its fleeing speed
uint32_t FleeingFromCreatureReaction(components::LivingAction& action);
/// 160 TURN_TO_FACE_CREATURE_REACTION: it turns to the creature and, once facing it, respects it (156)
uint32_t TurnToFaceCreatureReaction(components::LivingAction& action);
/// 167 MOVE_TOWARDS_CREATURE_REACTION: a step of its walk; there, within half a metre of its goal, it goes on in the
/// state the walk was for
uint32_t MoveTowardsCreatureReaction(components::LivingAction& action);
/// 167's validate: the wall hug's own (nothing), then villager_reactions::ReactionValidate
bool MoveTowardsCreatureValidate(components::LivingAction& action);

/// The walk to a point near the creature: villager::SetupMoveToWithHug for `state`, then its top state written straight
/// as 167 (no exit, entry, clip or speed), the state the walk is for kept
void SetupMoveToCreatureReaction(entt::entity villager, glm::vec2 goal, VillagerStates state);
void SetupMoveToCreatureReaction(entt::entity villager, const map_coords::MapCoords& goal, VillagerStates state);
/// After each turn of the states above: nothing while the villager is needed for worship (which sends it there) or
/// the reaction's object is no creature. A creature in a fight's arena: it stops reacting. (missing) the original then
/// takes up the fight's own reaction to watch it, which is not ported. Else, with a town, by its attitude to the
/// creature (effects::creature_reaction::StepFromAttitude): it stops, runs (159) or respects (155)
void UpdateReactiveStateFromAttitudeToCreature(entt::entity villager);

} // namespace openblack::ecs::villager_creature
