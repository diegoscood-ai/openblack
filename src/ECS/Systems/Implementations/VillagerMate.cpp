/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerMate.h"

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerHome.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerReactions.h"

namespace openblack::ecs::villager_mate
{
using components::LivingAction;

namespace
{
/// The turning step of the villager's look as it turns to the held villager: the normal one
constexpr uint32_t k_TurnLookMode = 1;

Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const ReactionInfo& RowOf(openblack::Reaction type)
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
}

/// A living villager: one with its state machine
bool IsVillagerEntity(entt::entity thing)
{
	return thing != entt::null && Entities().Valid(thing) && Entities().AllOf<components::Villager, LivingAction>(thing);
}
} // namespace

void HeldByHand(entt::entity villager, std::optional<PlayerNames> player)
{
	if (!player.has_value() || !IsVillagerEntity(villager))
	{
		return;
	}
	// only by its own player's hand, and only of breeding age
	if (villager::GetPlayerOf(villager) != player || !villager::IsSexuallyActive(villager))
	{
		return;
	}
	static_cast<void>(effects::reactions::CreateReaction(villager, openblack::Reaction::ReactToVillagerInHand, *player, false));
}

void SetupReactToVillagerInHand(entt::entity villager, entt::entity held, uint32_t reaction)
{
	// it takes the reaction up, then keeps the held villager as what it reacts to
	villager_reactions::AddReaction(villager, reaction, VillagerStates::WaitForMate);
	villager_reactions::SetSlotObject(villager, held);
}

uint8_t ReactToVillagerInHandPriority(entt::entity villager, uint32_t reaction, [[maybe_unused]] uint32_t other)
{
	const auto* found = effects::reactions::Find(reaction);
	if (found == nullptr || !IsVillagerEntity(found->initiator))
	{
		return 0;
	}
	const auto held = found->initiator;
	// the held one still in the hand, this one of breeding age, of the other sex by their infos, of the same player and
	// not held by a script: the breeder's call's priority
	if (villager::GetFinalState(held) != VillagerStates::InHand || !villager::IsSexuallyActive(villager))
	{
		return 0;
	}
	if (villager::InfoOf(villager).sex == villager::InfoOf(held).sex)
	{
		return 0;
	}
	if (villager::GetPlayerOf(held) != villager::GetPlayerOf(villager) || villager::IsScriptControlled(villager))
	{
		return 0;
	}
	return static_cast<uint8_t>(RowOf(openblack::Reaction::ReactToBreeder).priority);
}

uint32_t WaitForMate(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto held = villager_reactions::SlotObject(villager);
	// (openblack, guard) the original reads its object without asking: a reaction with no object stops
	if (held == entt::null || !Entities().Valid(held))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	const float distance = town_queries::GetDistanceInMetres(town_queries::PosOf(villager), town_queries::PosOf(held));
	if (!(distance < RowOf(openblack::Reaction::ReactToVillagerInHand).maxReactionDistance))
	{
		villager_reactions::StopReactingAndSetState(villager);
		return 1;
	}
	// it turns to the held one and, facing it and ready for a new clip, plays its state's clip
	if (villager::LookAtPos(villager, town_queries::PosOf(held), k_TurnLookMode) != 0 &&
	    Locator::livingActionSystem::value().VillagerIsReadyForNewAnimation(action, 1))
	{
		villager_animate::SetStateAnim(villager);
	}
	return 1;
}

} // namespace openblack::ecs::villager_mate
