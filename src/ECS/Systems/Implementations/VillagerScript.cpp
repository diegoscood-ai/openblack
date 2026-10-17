/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerScript.h"

#include <optional>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/ScriptAnimation.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Flocks.h"
#include "ECS/Registry.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Town/TownVillagers.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerHome.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerDrowning.h"
#include "ECS/VillagerScriptRules.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs::villager_script
{
using components::LivingAction;
using components::ScriptAnimation;
using components::Villager;
using villager::Index;
using villager::state_info::StateInfo;
namespace rules = villager_script_rules;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingAction* ActionOf(entt::entity villager)
{
	return Entities().TryGet<LivingAction>(villager);
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

/// SetAnim(n): the state's clip if it is another one, from its start when n != 0 and not dancing (IN_SCRIPT and the
/// script states are never a dance: not tested)
void SetAnim(entt::entity villager, int32_t n)
{
	villager_animate::PlayStateClip(villager, n != 0);
}
} // namespace

bool CanBeDirected(entt::entity villager)
{
	// IsAvailable and IsObjectInMap
	return villager::IsAvailable(villager) && villager::IsObjectInMap(villager);
}

void SetScriptState(entt::entity villager, VillagerStates state)
{
	auto* action = ActionOf(villager);
	if (action == nullptr)
	{
		return;
	}
	// IsAvailable and IsObjectInMap, else nothing
	if (!CanBeDirected(villager))
	{
		return;
	}
	// StorePreviousState
	villager::StorePreviousState(villager);
	// CallExitStateFunction(state), CallEntryStateFunction(state); both
	// results are ignored
	villager::CallExitStateFunction(villager, state);
	villager::CallEntryStateFunction(villager, state);
	// (approximate) The original's walk only advances from MOVE_TO_POS's state function, so a villager the script
	// takes out of it stops where it is; openblack's
	// PathfindingSystem steps every entity with a move tag whatever its state, so the tags go here
	if (villager::GetState(villager, Index::Top) != VillagerStates::MoveToPos)
	{
		Entities()
		    .Remove<components::MoveStateLinearTag, components::MoveStateOrbitTag, components::MoveStateExitCircleTag,
		            components::MoveStateStepThroughTag, components::MoveStateFinalStepTag, components::MoveStateArrivedTag>(
		        villager);
		Entities().Remove<components::WallHugObjectReference>(villager);
	}
	// SetAnim(1)
	SetAnim(villager, 1);
	// The counter = 0
	action->turnsUntilStateChange = 0;
}

void MoveTo(entt::entity villager, glm::vec2 goal)
{
	// only when it is in the map and not drowning, else nothing
	if (!villager::IsObjectInMap(villager) || IsDrowning(villager))
	{
		return;
	}
	// not there yet -> SetupMoveToPos(coords, IN_SCRIPT 4); else SetScriptState(IN_SCRIPT 4)
	if (!villager::AreWeThere(villager, goal, 0.0f))
	{
		villager::SetupMoveToPos(villager, goal, VillagerStates::InScript);
	}
	else
	{
		SetScriptState(villager, VillagerStates::InScript);
	}
}

void SetScriptAnimation(entt::entity villager, uint32_t clip, uint32_t plays)
{
	if (!Entities().AllOf<Villager>(villager))
	{
		return;
	}
	// The times (the first pop), then the clip (the second pop); only the simulation reads them
	Entities().AssignOrReplaceState<ScriptAnimation>(villager, static_cast<AnimId>(clip), plays);
}

int32_t ScriptClip(entt::entity villager)
{
	// clip 0 for none asked for
	const auto* animation = Entities().TryGet<const ScriptAnimation>(villager);
	return animation != nullptr ? static_cast<int32_t>(animation->clip) : 0;
}

bool HasPlayedScriptAnimation(entt::entity villager)
{
	// TOP 23 WAIT_FOR_ANIMATION -> 0; TOP 200 SCRIPT_PLAY_ANIM -> the times left == 0 (none without a clip asked for);
	// else 1
	const auto top = villager::GetState(villager, Index::Top);
	const auto* animation = Entities().TryGet<const ScriptAnimation>(villager);
	return rules::HasPlayedScriptClip(top, animation != nullptr ? animation->playsLeft : 0);
}

uint32_t InScript([[maybe_unused]] LivingAction& action)
{
	// No script reminder data -> create it (not ported); 1
	return 1;
}

uint32_t EnterInScript(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (villager::IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// No script reminder data, or it is not for `next` -> 1. The reminder data is not ported
	// (inferred: without it the villager has no walk to resume), so the branch that resumes it (SetupMoveToPos to
	// the stored goal or the reminder's point, next, and 0x23) is never taken
	(void)action;
	return 1;
}

uint32_t ExitInScript(LivingAction& action, VillagerStates next)
{
	const auto villager = EntityOf(action);
	// The circle-hug info is reset (openblack's WallHug has none); IsDancing,
	// result unused
	// IsScriptState(next) (state info table) -> 1. The script reminder data is created if missing and keeps
	// GetFinalState in mind: not ported. Then ExitNoChangeState(next): 1 if `next` is script-interruptable, is IN_HAND
	// or IsStateExitFunctionSameAs (the same exit as GetFinalState's, or not a final state); else 0
	const auto& info = StateInfo(next);
	const rules::StateRules nextRules {.isScriptState = villager::state_info::IsScriptState(info),
	                                   .isScriptInterruptable = villager::state_info::IsScriptInterruptable(info),
	                                   .isFinalState = villager::state_info::IsFinal(info)};
	const bool sameExit = villager::HaveSameExitFunction(villager::GetFinalState(villager), next);
	return rules::ScriptLetsGo(next, nextRules, sameExit) ? 1 : 0;
}

uint32_t ScriptPlayAnim(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto* animation = Entities().TryGet<ScriptAnimation>(villager);
	// None asked for -> nothing
	if (animation == nullptr)
	{
		return 1;
	}
	// No times left -> nothing; else one time less and PlayAnimThenSetState(times left ? 200 SCRIPT_PLAY_ANIM : 4
	// IN_SCRIPT, 1)
	const auto step = rules::StepScriptClip(animation->playsLeft);
	animation->playsLeft = step.playsLeft;
	if (step.then.has_value())
	{
		villager::PlayAnimThenSetState(villager, *step.then);
		// The script reminder data is created if missing (not ported)
	}
	return 1;
}

uint32_t EnterPlayAnim(LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Always a villager here. IsStateEntryFunctionSameAs(final, next) -> 1
	if (villager::IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// No script reminder data or it is not for `next` -> 1; the reminder branch (back to its walk, or its clip
	// again) is not ported (inferred: never taken without the reminder data)
	(void)action;
	return 1;
}

uint32_t ExitPlayAnim(LivingAction& action, VillagerStates next)
{
	// ExitInScript
	return ExitInScript(action, next);
}

void ReleaseFromScript(entt::entity villager)
{
	auto& registry = Entities();
	auto* v = registry.TryGet<Villager>(villager);
	if (v == nullptr)
	{
		return;
	}
	// While the map is being cleared nothing happens; openblack's map change releases nothing, so this never arises
	// here. Its flock: the flock's shepherd cleared when it is this one (openblack's flocks have no shepherd yet), then
	// it leaves the flock
	if (const auto flock = flocks::FlockOf(villager); flock != entt::null)
	{
		flocks::RemoveLiving(flock, villager, false);
	}
	const bool hasTown = v->town != entt::null && registry.Valid(v->town);
	// In the physics or in the hand: without a town, the vagrants' head
	if (Locator::dynamicsSystem::value().IsFlying(villager) || fire::traits::InHand(villager))
	{
		if (!hasTown)
		{
			town_villagers::AddToVagrants(villager);
		}
		return;
	}
	// life <= 0 -> VillagerDead(0, the player who last dropped it, which for a villager is always none, 0, 1); not a
	// death state -> SetTopState(15)
	if (!(v->life > 0.0f))
	{
		villager::VillagerDead(villager, DeathReason::None, std::nullopt, 0.0f, 1);
		const auto* action = registry.TryGet<const LivingAction>(villager);
		const auto top = action != nullptr ? action->states[static_cast<size_t>(LivingAction::Index::Top)] : 0;
		if (!(top >= 13 && top <= 17))
		{
			villager::SetTopState(villager, VillagerStates::Dead);
		}
		return;
	}
	// A town -> DecideWhatToDo
	if (hasTown)
	{
		if (auto* action = registry.TryGet<LivingAction>(villager); action != nullptr)
		{
			villager::DecideWhatToDo(*action);
		}
		return;
	}
	// The vagrants (unless there already), SetTopState(130), VagrantStart
	town_villagers::AddToVagrants(villager);
	villager::SetTopState(villager, VillagerStates::VagrantStart);
	villager::VagrantStart(villager);
}

} // namespace openblack::ecs::villager_script
