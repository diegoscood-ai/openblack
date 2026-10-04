/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/fwd.hpp>

namespace openblack::ecs
{

/// GameThing::ToBeDeleted (vt +0xC) per class, the common "this object goes" (the physics' -4R deletion 0x645B22, a sunk
/// animal, a drowned villager...): the class's own clean-up, then out of the physics and out of the registry.
///
/// - Villager::ToBeDeleted 0x7521B0: villager::ToBeDeletedOverride (DeleteDependancys 0x74FD60, then Living::ToBeDeleted
///   0x5EC0A0's StopReacting; ECS/Villager/VillagerDeath.h);
/// - Animal::ToBeDeleted 0x417B60 -> DeleteDependancys 0x417BA0: the animal AI forgets it (flock, prey, hunter);
/// - Tree::ToBeDeleted 0x74A210: out of its forest (fn_0053A220) and of the game's tree list [g_game+0x205CDC]; in
///   openblack a tree only carries its forest id, so nothing is left to unlink;
/// - Object::ToBeDeleted 0x636670: the rest (its shadow / attached thing at +0x44).
///
/// Then GameThing::ToBeDeleted 0x56FB70: already unavailable, nothing; `now` (its argument), deleted at once; else
/// marked Unavailable (flags +0xA bit 0) and put at the head of the dead list (g_game +0x205D1C), freed by
/// ProcessDeadList; its physics body stays until PhysicsObject::GameTurnUpdate drops the unavailable ones (0x645018).
/// (pending, Hito 3 step 3) Until every owner's readers ask IsAvailable the deferral is off (SetDeferredDeletion)
/// and the entity goes at once (out of the physics, then destroyed), as before. Trees go through DeleteTree at
/// once either way (pending: its unlinking and its destruction are one function, Trees.cpp).
void ToBeDeleted(entt::entity entity, bool now = false);

/// GameThing::IsAvailable (GameThing.h:361): a valid entity not marked Unavailable (entt::null: false)
[[nodiscard]] bool IsAvailable(entt::entity entity);

/// GameThing::ProcessDeadList 0x56FB10 / ProcessDead 0x56FAA0: from the head (the last marked first), each thing
/// marked before this pass; the first pass only notes it (+0xA bit 1), the next one frees it. `drain` (the argument 1
/// of GGame::Close 0x54EC80, ClearMap 0x552E58, LoadAllGame 0x558A9B) frees them all, again until the list is empty.
/// The turn calls it with false at 0x54E704. Nothing to do while the deferral is off.
void ProcessDeadList(bool drain);

/// (openblack) Hito 3 step 3 (documentacion/motor/dead_list.md): on once every owner's readers are zombie-safe
void SetDeferredDeletion(bool deferred);
[[nodiscard]] bool DeferredDeletion();

} // namespace openblack::ecs
