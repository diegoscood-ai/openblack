/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerDance.h"

#include <optional>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/SkeletalAnimation.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DanceMoves.h"
#include "ECS/Dances.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerDecide.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/Villager/VillagerStateInfo.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerScriptRules.h"
#include "ECS/WalkGoal.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerCreatureDance.h"

namespace openblack::ecs::villager_dance
{
using components::Dance;
using components::LivingAction;
using components::Transform;
using components::WallHug;
using villager::state_info::StateInfo;
namespace moves = dance_moves;
namespace rules = villager_script_rules;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

uint32_t InDance(LivingAction& action, const WalkStep& walk)
{
	return PerformDance(action, walk, VillagerStates::InDance);
}

uint32_t PerformDance(LivingAction& action, const WalkStep& walk, VillagerStates danceState, std::optional<uint32_t> move)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (transform == nullptr)
	{
		return 1;
	}
	const auto turn = dances::DancerTurnOf(registry, villager, map_coords::FromMetres(glm::xz(transform->position)),
	                                       villager::GetGameAngle(villager), move);
	// In no group it does nothing. TODO(dance): the script's reminder, made here when it has none, is not ported
	if (!turn.has_value())
	{
		return 1;
	}
	if (turn->facing.has_value())
	{
		villager::SetGameAngle(villager, *turn->facing);
	}
	// Worshipping in a creature's dance gives its town belief in the creature's player
	if (turn->worships)
	{
		const auto dance = dances::DanceOf(registry, villager);
		const auto& data = registry.Get<const components::Dance>(dance);
		villager_creature_dance::WorshipCreature(villager, data.owner, data.type);
	}
	if (turn->distance > 0.0f)
	{
		// It sets out for its place, round what is in the way when it is far off, to dance there once it is there; whether
		// the walk could be set out is not looked at
		const auto goal = map_coords::ToMetres(turn->place);
		if (turn->distance > dances::k_WalkRoundDistance)
		{
			static_cast<void>(villager::SetupMoveToWithHug(villager, goal, danceState));
		}
		else
		{
			static_cast<void>(villager::SetupMoveToPos(villager, goal, danceState));
		}
		SetWalkingToPlace(action);
		// The walk's first step is taken this turn
		static_cast<void>(walk(action));
	}
	return 1;
}

uint32_t MoveToDancePos(LivingAction& action, const WalkStep& walk)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	if (!dances::PlaceOf(registry, villager).has_value())
	{
		return 0;
	}
	if (const auto moved = dances::MovedPlace(registry, villager); moved.has_value())
	{
		if (auto* wallHug = registry.TryGet<WallHug>(villager); wallHug != nullptr)
		{
			walk_goal::Set(villager, wallHug->goal, map_coords::ToMetres(*moved));
		}
	}
	return walk(action);
}

uint32_t DanceWhileReacting(LivingAction& action, const WalkStep& walk)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto dance = dances::DanceOf(registry, villager);
	const auto* transform = registry.TryGet<const Transform>(villager);
	if (dance == entt::null || transform == nullptr)
	{
		return villager::DecideWhatToDo(action);
	}
	// standing on the dance's arrive point, away from its place, it walks straight to its place
	const auto at = map_coords::FromMetres(glm::xz(transform->position));
	const auto turn = dances::DancerTurnOf(registry, villager, at, villager::GetGameAngle(villager));
	const auto arrive = living_footpath::ArrivePosOf(dance, villager);
	if (turn.has_value() && turn->distance > 0.0f && at.x == arrive.x && at.z == arrive.z && at.altitude == arrive.altitude)
	{
		static_cast<void>(
		    villager::SetupMoveToPos(villager, map_coords::ToMetres(turn->place), VillagerStates::DanceWhileReacting));
		return 1;
	}
	return PerformDance(action, walk, VillagerStates::DanceWhileReacting);
}

uint32_t EnterInDance([[maybe_unused]] LivingAction& action, VillagerStates final, VillagerStates next)
{
	// Coming from a state with the same entry, accepted at once
	if (villager::IsStateEntryFunctionSameAs(final, next))
	{
		return 1;
	}
	// TODO(dance): with the script's reminder, a villager taken out of the dance on its way walks on to where it was
	// going; the reminder is not ported, so it is accepted as one without
	return 1;
}

uint32_t ExitInDance(LivingAction& action, VillagerStates next)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto& info = StateInfo(next);
	const rules::StateRules nextRules {.isScriptState = villager::state_info::IsScriptState(info),
	                                   .isScriptInterruptable = villager::state_info::IsScriptInterruptable(info),
	                                   .isFinalState = villager::state_info::IsFinal(info)};
	const bool sameExit = villager::HaveSameExitFunction(villager::GetFinalState(villager), next);
	const auto exit = rules::ExitDance(next, nextRules, sameExit);
	if (exit.leavesDance)
	{
		// TODO(dance): the script's reminder keeps the dance in mind, to go back to it after a reaction
		dances::RemoveDancer(registry, villager);
	}
	return exit.mayGo ? 1 : 0;
}

void SetWalkingToPlace(LivingAction& action)
{
	action.states.at(static_cast<size_t>(LivingAction::Index::Top)) = static_cast<uint8_t>(VillagerStates::MoveToDancePos);
	action.turnsSinceStateChange = 0;
}

int32_t DanceClip(entt::entity villager, int32_t walkClip, const std::function<uint32_t(uint32_t)>& random)
{
	auto& registry = Entities();
	const auto placed = dances::PlaceOf(registry, villager);
	if (!placed.has_value())
	{
		return moves::DanceClip({}, random);
	}
	const auto& dance = registry.Get<const Dance>(placed->dance);
	const auto& group = dance.groups.all[placed->group];
	std::optional<moves::Action> parentAction;
	if (group.parent.has_value() && *group.parent < dance.groups.all.size())
	{
		parentAction = static_cast<moves::Action>(dance.groups.all[*group.parent].move.action);
	}
	// The men's and the women's clips by the villager's info
	return moves::DanceClip(
	    {
	        .inGroup = true,
	        .stopped = dance.state == Dance::State::Stopped,
	        .action = group.move.action,
	        .first = group.move.first,
	        .parentAction = parentAction,
	        .rate = dance.rate,
	        .female = villager::InfoOf(villager).sex != SexType::Male,
	        .walkClip = walkClip,
	    },
	    random);
}

void PlayClipAgain(entt::entity villager)
{
	auto& registry = Entities();
	if (!registry.Valid(villager) || !registry.AllOf<components::Villager, LivingAction>(villager))
	{
		return;
	}
	// Its part in the move may want another clip, which takes over in time with the dance
	villager_animate::PlayStateClip(villager, true);
	// The clip starts over with the move once it has played through
	auto* skeletal = registry.TryGet<components::SkeletalAnimation>(villager);
	if (skeletal == nullptr || !skeletal->hasClip || !villager_clips::ClipMilliseconds(skeletal->clip).has_value())
	{
		return;
	}
	auto& action = registry.Get<LivingAction>(villager);
	const auto move = villager_clips::OnDanceMove(action, skeletal->clip);
	action.turnsSinceStateChange = static_cast<uint16_t>(move.turnsSinceStateChange);
	if (move.restart)
	{
		skeletal->time = 0.0f;
	}
}

} // namespace openblack::ecs::villager_dance
