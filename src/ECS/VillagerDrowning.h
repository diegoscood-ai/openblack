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

namespace openblack::ecs
{
namespace components
{
struct LivingAction;
}

/// Sinking and drowning, per class, like the original (research dev\tmp_dis\agua\objects_physics.md §3, §6, §7):
///
/// PhysicsObject::GameTurnUpdate (0x645A01), each substep: when the body is awake, its centre is under half its radius
/// and its density is over 1, the object's HasSunk (vt +0x7B8) is asked; if it says yes the body stops (v = 0, L = 0)
/// and EndPhysics runs as if it had come to rest. The villager then stands in the sea in DROWNING (16) for
/// GVillagerInfo::drowningTime turns (600 = 60 s) and dies (DEATH_REASON_PLAYER_INTERACTION_DROWN). The drowning
/// villager's clip is the state's (info.dat: 252 P_DROWNING, whose events play the swim splash 157 and the drowning
/// scream 134 through ECS/AnimationSounds).

/// HasSunk (vt +0x7B8):
/// - Object::HasSunk 0x637470: no (rocks, trees, pots... sink on until the -4R deletion, ToBeDeleted);
/// - Living::HasSunk 0x5ED370 (animals): SetDying, LIVING_DEAD and ToBeDeleted(0) -> the entity is gone;
/// - Villager::HasSunk 0x750AB0: stateCounter = drowningTime and DROWNING (16).
/// TODO(creature): both Living ones first tell the creature to learn from the player who dropped it
/// (ConsiderMakingCreatureMimicPlayer, DETECTED_PLAYER_ACTION_THROW_IN_THE_SEA 0x15).
bool HasSunk(entt::entity entity);

/// Villager::EndPhysics (0x5F0A60), the water branch (0x5F0BAF-0x5F0C9D): the villager came to rest (or sank) on a cell
/// with the water bit (MapCoords::IsWater, the shallow shore too): alive -> stateCounter = drowningTime and DROWNING;
/// otherwise VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN). No LANDED.
void VillagerEndPhysicsInWater(entt::entity villager);

/// Villager::Drowning (0x76A780), the DROWNING state's function, once per game turn: --stateCounter, and at 0
/// VillagerDead(DEATH_REASON_PLAYER_INTERACTION_DROWN, the player who dropped it or lastPlayerToInteract, 0.01, true).
uint32_t VillagerDrowningState(components::LivingAction& action);

/// IsDrowning (vt +0x17C), the script's GET_PROPERTY(DROWNING):
/// - Villager::IsDrowning 0x756B30: its state is DROWNING (16);
/// - Object::IsDrowning 0x63A780: in physics (flag 0x40 of +0x24) and the physics body's centre (po +0xCC) under y 0;
/// - GameThingWithPos::IsDrowning 0x4052D0: no.
[[nodiscard]] bool IsDrowning(entt::entity entity);

} // namespace openblack::ecs
