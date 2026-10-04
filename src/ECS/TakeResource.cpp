/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TakeResource.h"

#include <spdlog/spdlog.h>

#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Physics/PhysicsObjects.h"
// TODO(Intro HEAD): #include "Help/HelpProfile.h"

using namespace openblack;
using namespace openblack::ecs;

void take_resource::TriggerSupplyHelpIfThrownByMe(entt::entity object)
{
	// 0x73376D / 0x77E7B6: Object +0x24 & 0x40 (in the physics) and SearchForPhysicsObject 0x646950 (the list
	// 0xD47814, 0x1DC bytes each, by +0x18 the object) -> both are PhysicsObjects::Find
	const auto* po = physics::PhysicsObjects::Find(object);
	// 0x733780..0x733796 / 0x77E7D2..0x77E7DD: PhysicsObject +0x24 (its GInterfaceStatus) == MyInterfaceStatus 0x555880
	if (po == nullptr || !po->byPlayer)
	{
		return;
	}
	// 0x7337A4..0x7337A6 / 0x77E7EA..0x77E7EC: g_game +0x250060 (the HelpProfile) ->Trigger(6) 0x5C46E0
	// TODO(Intro HEAD): help_profile::Trigger(help_profile::Event::Supply);
	SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "HelpProfile::Trigger(SUPPLY) for {} (pending: help_profile)",
	                    static_cast<uint32_t>(object));
}

bool take_resource::StoragePit(entt::entity store, entt::entity object, const pot_resource::Dropper& is)
{
	// 0x73375A..0x733767: ebp = is ? is->GetPlayer() (vt +0x1C) : NULL, read first. A NULL GPlayer is openblack's
	// NEUTRAL (as the other CreateReaction callers)
	const PlayerNames player = is.hasInterface ? is.player : PlayerNames::NEUTRAL;
	TriggerSupplyHelpIfThrownByMe(object);
	// 0x7337AB..0x7337AF: DoDeleteObjectAndTakeResource(object, is) 0x63A940 (this = the store; void in the original,
	// what was taken is not used)
	object_delivery::DoDeleteObjectAndTakeResource(store, object, is);
	// 0x7337B4..0x7337BA: Reaction::CreateReaction(this, 0x16, player, 1) 0x6E3D70: REACTION 22
	// REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, initiated by the store, stamped with the turn (the last argument 1,
	// 0x6E3DE9..0x6E3DFA); the returned reaction is dropped (add esp, 0x10 at 0x7337BF)
	effects::reactions::CreateReaction(store, Reaction::ReactToHandPuttingStuffInStoragePit, player, true);
	// 0x7337C5: mov eax, 1
	return true;
}
