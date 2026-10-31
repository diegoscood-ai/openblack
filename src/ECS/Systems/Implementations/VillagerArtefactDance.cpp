/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerArtefactDance.h"

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/ArtefactRules.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Dances.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::villager_artefact_dance
{
using components::Dance;
using components::LivingAction;
using components::Transform;
namespace rules = artefact_rules;
using ClearAreaFilter = systems::TownSystemInterface::ClearAreaFilter;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

uint32_t WaitForArtefactDance(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto dance = dances::DanceOf(registry, villager);
	if (dance == entt::null)
	{
		static_cast<void>(villager::SetTopState(villager, VillagerStates::DecideWhatToDo));
		return 1;
	}
	const auto& data = registry.Get<const Dance>(dance);
	const auto at = glm::xz(registry.Get<const Transform>(villager).position);
	const auto middle = map_coords::ToMetres(data.place);
	// It keeps a little more than its own width clear about itself
	const float room = object::Get2DRadius(villager) * rules::k_WaitingRoom;
	const float distance = glm::distance(middle, at);
	const auto& towns = Locator::townSystem::value();
	// Whether it has room where it stands is only asked on its first turn of waiting close by
	const bool askRoom = distance <= rules::k_WaitingReach && action.turnsUntilStateChange == 0;
	const bool roomHere = askRoom && towns.CheckForClearArea(at, room, ClearAreaFilter::AnyObject, villager);
	switch (rules::WaitingTurn(distance, action.turnsUntilStateChange, roomHere,
	                           villager::InfoOf(villager).timeToWaitForArtifactDance))
	{
	case rules::Waiting::WalkCloser:
	{
		// To a clear spot up to 12 m from the artefact, on its own side of it, a little to one side or the other
		const float spread = game_random::GameFloatRand(rules::k_WaitingSpread);
		const float angle = gutils::Get3DAngleFromXZ(middle, at) + spread - rules::k_WaitingHalfSpread;
		const float away = game_random::GameFloatRand(rules::k_WaitingReach);
		const auto target = map_coords::ToMetres(data.place + gutils::GetPosFromAngle(angle, away));
		if (const auto spot = towns.FindClearArea(target, rules::k_WaitingReach, rules::k_WaitingRoomStep, room,
		                                          ClearAreaFilter::AnyObject, villager);
		    spot.has_value())
		{
			static_cast<void>(villager::SetupMoveToPos(villager, *spot, VillagerStates::WaitForArtifactDance));
		}
		break;
	}
	case rules::Waiting::MakeRoom:
		if (const auto spot = towns.FindClearArea(at, rules::k_WaitingReach, rules::k_WaitingRoomStep, room,
		                                          ClearAreaFilter::AnyObject, villager);
		    spot.has_value())
		{
			static_cast<void>(villager::SetupMoveToPos(villager, *spot, VillagerStates::WaitForArtifactDance));
		}
		break;
	case rules::Waiting::Wait:
		++action.turnsUntilStateChange;
		break;
	case rules::Waiting::StartDance:
	{
		++action.turnsUntilStateChange;
		auto& started = registry.Get<Dance>(dance);
		started.state = Dance::State::Dancing;
		started.startTurn = villager::CurrentTurn();
		break;
	}
	}
	return 1;
}

uint32_t EnterWaitForArtefactDance(LivingAction& action, [[maybe_unused]] VillagerStates final,
                                   [[maybe_unused]] VillagerStates next)
{
	action.turnsUntilStateChange = 0;
	return 1;
}

uint32_t ArtefactDance(LivingAction& action, const villager_dance::WalkStep& walk)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto dance = dances::DanceOf(registry, villager);
	if (dance != entt::null)
	{
		// One standing right on the dance's own spot, away from its place, walks straight to its place
		const auto& place = registry.Get<const Dance>(dance).place;
		const auto at = map_coords::FromMetres(glm::xz(registry.Get<const Transform>(villager).position));
		const auto turn = dances::DancerTurnOf(registry, villager, at, villager::GetGameAngle(villager));
		if (turn.has_value() && turn->distance > 0.0f && at.x == place.x && at.z == place.z)
		{
			static_cast<void>(
			    villager::SetupMoveToPos(villager, map_coords::ToMetres(turn->place), VillagerStates::ArtifactDance));
			return 1;
		}
	}
	return villager_dance::PerformDance(action, walk, VillagerStates::ArtifactDance);
}

uint32_t ExitDance(LivingAction& action, VillagerStates next)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	if (!villager::IsStateExitFunctionSameAs(villager, next) && next != VillagerStates::InDance)
	{
		dances::RemoveDancer(registry, villager);
	}
	return 1;
}

} // namespace openblack::ecs::villager_artefact_dance
