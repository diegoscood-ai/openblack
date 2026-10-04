/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/fwd.hpp>

#include "ECS/PotResource.h"

// Object::DeleteObjectAndTakeResource (vt +0x684 of the receiver, (Object*, GInterfaceStatus*)): a structure takes the
// object given to it and the object goes. Its callers ask IsResourceStore (vt +0x680) first: Tree::ApplyThisToMapCoord
// 0x74C008 (put down on a store: MapCoords::IsWithinBuilingSite 0x605250; 1 -> returns 3) and
// Tree::ReactToPhysicsImpact 0x74B6F9 (it hit one: GetGameObjectWhoHitMe 0x644F00, `is` the PhysicsObject's +0x24).
// Object 0x63A930 returns 0 (nobody takes it); StoragePit 0x733750 and WorshipSite 0x77E7B0 (here and
// worship::site::DeleteObjectAndTakeResource) wrap the shared Object::DoDeleteObjectAndTakeResource 0x63A940 (the
// resource moved and the object deleted: ecs::object_delivery, ECS/ObjectDelivery.h). The other overrides
// (MultiMapFixed 0x52F460, Pot 0x66DD30, Scaffold 0x6EAEC0, Workshop 0x779F20) are (pending).

namespace openblack::ecs::take_resource
{
/// 0x73376D..0x7337A6 (StoragePit) = 0x77E7B6..0x77E7EC (WorshipSite), the same code in both: the object is in the
/// physics (Object +0x24 & 0x40) with a PhysicsObject (SearchForPhysicsObject 0x646950) whose interface (+0x24) is the
/// local one (GGame::MyInterfaceStatus 0x555880) -> HelpProfile::Trigger(6 SUPPLY) 0x5C46E0 (g_game +0x250060). The
/// receiver's own `is` is not what is tested. openblack: PhysicsObjects::Find and its byPlayer (the local hand threw
/// it, directly or through what it hit; (approximate) one local interface, so "the hand's" is "MyInterfaceStatus").
/// TODO(Intro HEAD): the trigger is only logged until help_profile::Trigger lands (TakeResource.cpp).
void TriggerSupplyHelpIfThrownByMe(entt::entity object);

/// StoragePit::DeleteObjectAndTakeResource 0x733750 (object, is): the player of `is` (is ? is->GetPlayer() : NULL,
/// 0x73375A..0x733767, before anything else), TriggerSupplyHelpIfThrownByMe(object), DoDeleteObjectAndTakeResource(
/// object, is) 0x7337AF, then Reaction::CreateReaction(this, 0x16 REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, that
/// player, 1) 0x7337BA (its result is not used). Returns 1 (0x7337C5).
bool StoragePit(entt::entity store, entt::entity object, const pot_resource::Dropper& is);
} // namespace openblack::ecs::take_resource
