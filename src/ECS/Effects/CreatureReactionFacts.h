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

#include <entt/entity/fwd.hpp>
#include <glm/vec3.hpp>

#include "ECS/Effects/VillagerCreatureRules.h"

// What every living being reacting to a creature reads of the game, for the rules of
// ecs::effects::creature_reaction: the villagers' (Implementations/VillagerCreature) and the animals'
// (ECS/AnimalFlee.cpp). Wiki: docs/wiki/beings/villager-reactions.md#a-creature-reaction-6,
// docs/wiki/beings/animal-life.md#reactions.

namespace openblack::ecs::creature_reaction_facts
{
/// The thing is a creature
[[nodiscard]] bool IsCreature(entt::entity thing);

/// What the two turn counts of a living being reacting to `object` read: the reaction info's distances and its turns
/// for normal things (to react, or before reacting again), and for a creature how impressive the last action it
/// carried out to its end and the action of the plan it holds are. The town's attitude is left to the caller: a
/// villager's town's, none for an animal
[[nodiscard]] effects::creature_reaction::TurnsFacts TurnsFactsOf(entt::entity object, uint8_t type, float distance,
                                                                  bool again);

/// The way a creature goes, as a living being that runs from it sees it (effects::flee::CreatureMovement of its body's
/// yaw). Needs a Transform
[[nodiscard]] glm::vec3 MovementOf(entt::entity creature);
} // namespace openblack::ecs::creature_reaction_facts
