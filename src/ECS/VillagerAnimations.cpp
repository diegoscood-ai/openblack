/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerAnimations.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/MobileDrawing.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerAnimate.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerSpeed.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs
{
using namespace components;
using villager_animation::Transition;

namespace
{
const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates TopState(entt::entity villager)
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	return action != nullptr ? static_cast<VillagerStates>(action->states[static_cast<size_t>(LivingAction::Index::Top)])
	                         : VillagerStates::InvalidState;
}

/// The into / out-of clip of a state's transition: into = entering the state; -1 = none
int32_t TransitionAnim(Transition transition, entt::entity entity, bool into, VillagerStates other, VillagerStates self)
{
	const auto current = static_cast<AnimId>(villager_animate::CurrentClip(entity));
	const auto clip =
	    villager_animation::TransitionClip(transition, into, other == self, villager_animation::IsDeathState(other), current);
	return clip.has_value() ? static_cast<int32_t>(*clip) : -1;
}

SkeletalAnimation& AnimationOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animation = registry.TryGet<SkeletalAnimation>(entity); animation != nullptr)
	{
		return *animation;
	}
	return registry.Assign<SkeletalAnimation>(entity);
}

/// villager_animate::SetAnim, giving the villager its clip component first when it has none
void SetAnim(entt::entity entity, int32_t clip, bool reset)
{
	if (clip < 0)
	{
		return;
	}
	AnimationOf(entity);
	villager_animate::SetAnim(entity, clip, reset);
}

/// The state's clip from the start; negative ids (not drawn) keep the old clip
void SetStateAnim(entt::entity entity)
{
	villager_animate::SetStateAnim(entity);
}

/// The out-of clip for a given current state
int32_t OutOfClip(entt::entity entity, VillagerStates current, VillagerStates next)
{
	if (StateInfo(next).field0xf0 != 0 || static_cast<size_t>(current) >= 255)
	{
		return -1;
	}
	const auto out = TransitionAnim(villager_animation::TransitionOf(current), entity, false, next, current);
	if (out != -1)
	{
		// the flags 0x800 / 0x1000
		AnimationOf(entity).transitionFlags |= 0x1800;
	}
	return out;
}
} // namespace

int32_t VillagerCallOutOfAnimation(entt::entity entity, VillagerStates next)
{
	return OutOfClip(entity, TopState(entity), next);
}

void VillagerApplyStateClips(entt::entity entity, VillagerStates entered, int32_t out)
{
	const auto top = TopState(entity);
	// the state's speed with no test (its own skips, script-controlled or dancing, are inside SetVillagerStateSpeed)
	SetVillagerStateSpeed(entity);
	if (out != -1)
	{
		SetAnim(entity, out, true);
		return;
	}
	SetStateAnim(entity);
	if (static_cast<size_t>(top) >= 255)
	{
		return;
	}
	// the into clip: the TOP's function with (1, entered): the new top state, or the destination when both are set
	const auto into = TransitionAnim(villager_animation::TransitionOf(top), entity, true, entered, top);
	if (into != -1)
	{
		// flags = (flags | 0x800) & ~0x1000
		auto& flags = AnimationOf(entity).transitionFlags;
		flags = static_cast<uint16_t>((flags | 0x800) & ~0x1000);
		SetAnim(entity, into, true);
	}
}

void OnVillagerStateChanged(entt::entity entity, VillagerStates previous, VillagerStates next)
{
	const auto out = OutOfClip(entity, previous, next);
	VillagerApplyStateClips(entity, next, out);
}

bool VillagerWaitsForTransition(entt::entity entity, uint16_t turnsSinceStateChange)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* animation = registry.TryGet<SkeletalAnimation>(entity);
	if (animation == nullptr || (animation->transitionFlags & 0x800) == 0)
	{
		return false;
	}
	// ready for a new clip: turns in the state * 100 ms >= the clip's duration (no resources, in the
	// unit tests: no duration)
	const uint32_t playTime = animation->hasClip ? villager_clips::ClipMilliseconds(animation->clip).value_or(0) : 0;
	if (!villager_clips::HasPlayed(turnsSinceStateChange, game_clock::k_MsPerTurn, playTime, 1))
	{
		return true;
	}
	// the into / out-of clip is over, still skipping the state logic this turn
	auto* action = registry.TryGet<LivingAction>(entity);
	const auto state = TopState(entity);
	bool replayed = false;
	if ((animation->transitionFlags & 0x1000) != 0 && static_cast<size_t>(state) < 255 &&
	    villager_animation::TransitionOf(state) != Transition::None)
	{
		// an out-of clip ended and the state has an into clip: the original calls SetAnim(the state's clip, into), so the
		// state's own clip restarts in "transition" mode for one more cycle and the into clip is never shown
		const auto into = TransitionAnim(villager_animation::TransitionOf(state), entity, true, state, state);
		if (into != -1)
		{
			const auto clip = villager_animate::StateClip(entity);
			SetAnim(entity, clip, true);
			AnimationOf(entity).time = 0.0f;
			AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x1000);
			replayed = true;
		}
	}
	if (!replayed)
	{
		// no out-of clip, no into function or no into clip: SetStateAnim and the wait ends; only 0x800 is
		// cleared, 0x1000 stays
		SetStateAnim(entity);
		AnimationOf(entity).transitionFlags &= static_cast<uint16_t>(~0x800);
	}
	if (action != nullptr)
	{
		action->turnsSinceStateChange = 0;
	}
	return true;
}

int32_t VillagerLandedClip(uint8_t landType, bool carriesNothing)
{
	if ((landType & 3) == 0)
	{
		return static_cast<int32_t>(carriesNothing ? AnimId::PLandedFromFeet : AnimId::PLandedFromFeetCarryObject);
	}
	return static_cast<int32_t>((landType & 3) == 1 ? AnimId::PLanded : AnimId::PLandedFromBack);
}

bool VillagerAnimationDone(entt::entity entity, uint16_t turnsSinceStateChange)
{
	const auto* animation = Locator::entitiesRegistry::value().TryGet<const SkeletalAnimation>(entity);
	if (animation == nullptr || !animation->hasClip)
	{
		return true;
	}
	// a clip that is not loaded (or no resources, in the unit tests) has no length: it has played
	return villager_clips::HasPlayed(turnsSinceStateChange, game_clock::k_MsPerTurn,
	                                 villager_clips::ClipMilliseconds(animation->clip).value_or(0), 1);
}

namespace
{
/// What the hand or the physics does to a villager as its state changes: it stops walking and is drawn where it is.
/// Its LivingAction, or null for what is not a villager
LivingAction* StopForHandOrPhysics(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(entity);
	if (action == nullptr || !registry.AllOf<Villager>(entity))
	{
		return nullptr;
	}
	registry.Remove<MoveStateLinearTag, MoveStateOrbitTag, MoveStateExitCircleTag, MoveStateStepThroughTag,
	                MoveStateFinalStepTag, MoveStateArrivedTag>(entity);
	// the end of physics / the hand: drawn at its position, no slide from where it was
	NotifyTeleported(entity);
	return action;
}
} // namespace

void SetVillagerState(entt::entity entity, VillagerStates state)
{
	if (auto* action = StopForHandOrPhysics(entity); action != nullptr)
	{
		Locator::livingActionSystem::value().VillagerSetState(*action, LivingAction::Index::Top, state, true);
	}
}

void SetVillagerStateWithExit(entt::entity entity, VillagerStates state)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* action = registry.TryGet<LivingAction>(entity);
	if (action == nullptr || !registry.AllOf<Villager>(entity))
	{
		return;
	}
	// the old state's exit first, as any change of the top state runs it; already in the state: nothing, as the hand's
	// and the flight's own exits let no villager into the same state again
	const auto& states = Locator::livingActionSystem::value();
	if (states.VillagerGetState(*action, LivingAction::Index::Top) != state)
	{
		states.VillagerSetState(*action, LivingAction::Index::Top, state, false);
	}
	// then the walk dropped and the redraw, whatever the state
	StopForHandOrPhysics(entity);
}

void UpdateVillagerAnimations()
{
	auto& registry = Locator::entitiesRegistry::value();
	// not drawn in the states whose clip is -4 (the current state). A zombie (Unavailable) is left as it is in this
	// loop and the next: its deletion takes it off the living list
	std::vector<entt::entity> hide;
	std::vector<entt::entity> show;
	registry.Each<const Villager, const SkeletalAnimation>(
	    [&](entt::entity entity, const Villager&, const SkeletalAnimation& animation) {
		    const auto state = TopState(entity);
		    const bool hidden = villager_animate::IsHiddenByState(state);
		    if (hidden && animation.hiddenMesh == 0 && registry.AllOf<Mesh>(entity))
		    {
			    hide.push_back(entity);
		    }
		    else if (!hidden && animation.hiddenMesh != 0)
		    {
			    show.push_back(entity);
		    }
	    },
	    entt::exclude<Unavailable>);
	for (const auto entity : hide)
	{
		registry.Get<SkeletalAnimation>(entity).hiddenMesh = registry.Get<Mesh>(entity).id;
		registry.Remove<Mesh>(entity);
	}
	for (const auto entity : show)
	{
		auto& animation = registry.Get<SkeletalAnimation>(entity);
		registry.Assign<Mesh>(entity, animation.hiddenMesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		animation.hiddenMesh = 0;
	}
	// moving states (info field0x14) play the clip by the ground the villager's whole speed covers, whether or not it
	// moves (ecs::UpdateAnimations)
	registry.Each<const Villager, SkeletalAnimation>(
	    [&registry](entt::entity entity, const Villager&, SkeletalAnimation& animation) {
		    const auto state = TopState(entity);
		    const bool movingState = static_cast<size_t>(state) < 255 && StateInfo(state).field0x14 != 0;
		    const auto* wallHug = registry.TryGet<const WallHug>(entity);
		    animation.groundSpeed =
		        movingState && wallHug != nullptr ? std::optional<uint16_t>(WholeSpeed(*wallHug)) : std::nullopt;
	    },
	    entt::exclude<Unavailable>);
}

} // namespace openblack::ecs
