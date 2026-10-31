/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TakeResource.h"

#include "Creature/LocalPlayer.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/ObjectDelivery.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "Help/HelpProfile.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;

void take_resource::TriggerSupplyHelpIfThrownByMe(entt::entity object)
{
	// in physics (DynamicsSystem::IsFlying; a resting proxy keeps its list entry but is not in physics), then its
	// physics object (DynamicsSystem::Find)
	if (!Locator::dynamicsSystem::value().IsFlying(object))
	{
		return;
	}
	const auto* po = Locator::dynamicsSystem::value().Find(object);
	// the physics object's interface is the local one
	if (po == nullptr || po->throwerPlayer != openblack::creature::LocalPlayer())
	{
		return;
	}
	// the help profile's trigger 6 (SUPPLY)
	help_profile::Trigger(help_profile::Event::Supply);
}

bool take_resource::StoragePit(entt::entity store, entt::entity object, const pot_resource::Dropper& is)
{
	// the interface's player, read first; none without an interface
	const auto player = is.hasInterface ? std::optional(is.player) : std::nullopt;
	TriggerSupplyHelpIfThrownByMe(object);
	// the store takes it (what was taken is not used)
	object_delivery::DoDeleteObjectAndTakeResource(store, object, is);
	// REACTION 22 REACT_TO_HAND_PUTTING_STUFF_IN_STORAGE_PIT, initiated by the store, stamped with the turn (the last
	// argument); the returned reaction is dropped
	effects::reactions::CreateReaction(store, Reaction::ReactToHandPuttingStuffInStoragePit, player, true);
	return true;
}
