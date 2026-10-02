/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/


#include "VillagerReactions.h"

#include <fmt/format.h>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerFire.h"
#include "VillagerShield.h"
#include "VillagerTeleport.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
void VillagerReaction(entt::entity villager, const effects::reactions::Reaction& reaction, float /*distance*/)
{
	switch (reaction.type)
	{
	case openblack::Reaction::ReactToFire:
		villager_fire::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToTeleport:
		villager_teleport::ApplyReaction(villager, reaction);
		break;
	case openblack::Reaction::ReactToMagicShield:
		villager_shield::ApplyReaction(villager, reaction);
		break;
	default:
		break;
	}
}

/// The Villager handler from the start (before any map load too); Register() sets it again at each load
const bool k_VillagerHandlerRegistered = [] {
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
	return true;
}();
} // namespace

void villager_reactions::Register()
{
	effects::reactions::SetLivingReactionHandler(effects::reactions::LivingClass::Villager, &VillagerReaction);
}

uint32_t villager_reactions::SetTopState(entt::entity villager, VillagerStates state)
{
	const auto result = villager::SetTopState(villager, state);
	if (result != villager::k_ExitRefused)
	{
		Locator::entitiesRegistry::value()
		    .Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
		            MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
	}
	return result;
}

void villager_reactions::PopFromPrevious(entt::entity villager)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	// 0x751E57..0x751E68: Infos[stored] +0x30 (0xDB9E98, file 0x20). Nothing stored is row 0, whose resume is 0 in
	// info.dat: SetTopState(0) = INVALID_STATE (Living::InvalidState 0x5EC1D0 then returns 0 every turn), as the original
	const auto stored = static_cast<VillagerStates>(action->states.at(static_cast<size_t>(LivingAction::Index::Previous)));
	const auto next = villager::state_info::ResumeState(villager::state_info::StateInfo(stored));
	// 0x751E72 SetTopState (vt +0x8E8); 0x751E78..0x751E8A: 0x2E (an exit refused) -> raw LivingAction::SetState(0, 163)
	// 0x5ECC90 (index 0: +0x90 = 0 too)
	const auto result = SetTopState(villager, next);
	action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr)
	{
		return;
	}
	if (result == villager::k_ExitRefused)
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::DecideWhatToDo);
		action->turnsSinceStateChange = 0;
	}
	// 0x751E8F..0x751E99: raw LivingAction::SetState(2, 0)
	action->states.at(static_cast<size_t>(LivingAction::Index::Previous)) = 0;
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("PopFromPrevious stored {} -> resume {} = {:#x}", static_cast<int>(stored),
		                                      static_cast<int>(next), result));
	}
}

void villager_reactions::ResetStateAfterReacting(entt::entity villager)
{
	// 0x751E13
	PopFromPrevious(villager);
	// 0x751E18..0x751E48: GetFinalState (vt +0xB04) reactive (Infos +0xC8 0xDB9F30) -> SetTopState(0xA3) (vt +0x8E8)
	if (villager::state_info::IsReactive(villager::state_info::StateInfo(villager::GetFinalState(villager))))
	{
		SetTopState(villager, VillagerStates::DecideWhatToDo);
	}
}

void villager_reactions::StopReactingAndSetState(entt::entity villager)
{
	// 0x5F11C5: ResetStateAfterReacting (vt +0x9A0); 0x5F11CB..0x5F11D9: +0x94 != 0 -> StopReacting (vt +0x998)
	ResetStateAfterReacting(villager);
	if (IsReacting(villager))
	{
		StopReacting(villager);
	}
}

bool villager_reactions::ReactionValidate(LivingAction& action)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// +0xBC and the type of +0x94 (+0x24): the reaction it follows, kept by VillagerFire.cpp or VillagerTeleport.cpp
	auto object = entt::entity(entt::null);
	auto type = openblack::Reaction::None;
	if (villager_fire::ReactionObject(villager) != entt::null)
	{
		object = villager_fire::ReactionObject(villager);
		type = openblack::Reaction::ReactToFire;
	}
	else if (villager_teleport::ReactionObject(villager) != entt::null)
	{
		object = villager_teleport::ReactionObject(villager);
		type = openblack::Reaction::ReactToTeleport;
	}
	else if (villager_shield::ReactionObject(villager) != entt::null)
	{
		object = villager_shield::ReactionObject(villager);
		type = openblack::Reaction::ReactToMagicShield;
	}
	// 0x756A03..0x756A15: no object, or IsAvailable (vt 0x2C) != 1 -> PopFromPrevious (0x756A3F). The shield's object is
	// the spell itself, which is no fire object: its own availability test (and nothing holds a spell, so the in-hand
	// branch below cannot fire for it)
	if (type == openblack::Reaction::ReactToMagicShield)
	{
		const bool gone = !villager_shield::IsReactionObjectAvailable(villager);
		if (gone)
		{
			if (villager::TraceOn(villager))
			{
				villager::Trace(villager, "ReactionValidate: the shield spell went -> PopFromPrevious");
			}
			PopFromPrevious(villager);
		}
		return !gone;
	}
	bool pop = object == entt::null || !fire::traits::IsAvailable(object);
	// 0x756A17..0x756A3B: ReactionInfo[type].whetherReactionFinishesIfInitiatorInHand && object +0x24 & 4 (in the hand)
	if (!pop)
	{
		const auto& info = Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
		pop = info.whetherReactionFinishesIfInitiatorInHand != 0 && fire::traits::InHand(object);
	}
	if (pop)
	{
		if (villager::TraceOn(villager))
		{
			villager::Trace(villager, "ReactionValidate: the reaction's object went -> PopFromPrevious");
		}
		PopFromPrevious(villager);
	}
	return !pop;
}

bool villager_reactions::IsReacting(entt::entity villager)
{
	return villager_fire::IsReacting(villager) || villager_teleport::IsReacting(villager) ||
	       villager_shield::IsReacting(villager);
}

void villager_reactions::StopReacting(entt::entity villager)
{
	// Villager::StopReacting 0x7637D0: TOP 203 DANCE_WHILE_REACTING (0x7637D8) and IsDancing (vt +0x978) ->
	// RemoveFromDance(1) (vt +0xB08). TODO(dance): 203 has no state function in openblack, so no villager is there.
	// Then Living::StopReacting 0x5F1140 (0x7637F8) for the reaction it follows: the fire's, the teleport's or the
	// shield's
	if (villager::TraceOn(villager) && IsReacting(villager))
	{
		villager::Trace(villager, "StopReacting");
	}
	villager_fire::StopReacting(villager);
	villager_teleport::StopReacting(villager);
	villager_shield::StopReacting(villager);
}

uint32_t villager_reactions::ExitReaction(LivingAction& action, VillagerStates next)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.ToEntity(action);
	// 0x7527A3..0x7527A7: CircleHugInfo::Reset(this +0x70, this) 0x60A9F0: no hugged object, TurnsToObj 0xFF.
	// (aproximado) openblack's WallHugObjectReference
	registry.Remove<WallHugObjectReference>(villager);
	// 0x7527AC..0x7527C9: IsReactiveState(next) inline (Infos +0xC8 0xDB9F30, file 0xB8); 0 -> StopReacting (vt +0x998)
	const bool reactive = villager::state_info::IsReactive(villager::state_info::StateInfo(next));
	if (villager::TraceOn(villager))
	{
		villager::Trace(villager, fmt::format("ExitReaction({}) reactive {}", static_cast<int>(next), reactive ? 1 : 0));
	}
	if (!reactive)
	{
		StopReacting(villager);
	}
	// 0x7527D5
	return 1;
}
