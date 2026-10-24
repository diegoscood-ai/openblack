/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LivingFootpath.h"

#include <glm/vec2.hpp>

#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Fields.h"
#include "ECS/FishFarms.h"
#include "ECS/FootpathWalkRules.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/AbodeVillagers.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Villager/VillagerCore.h"
#include "ECS/Villager/VillagerScript.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Worship/WorshipSite.h"

namespace openblack::ecs::living_footpath
{
using components::LivingFootpath;

namespace
{
Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

LivingFootpath& FootpathOf(entt::entity living)
{
	auto& registry = Entities();
	if (auto* fp = registry.TryGet<LivingFootpath>(living); fp != nullptr)
	{
		return *fp;
	}
	return registry.Assign<LivingFootpath>(living);
}

/// The goal in metres. (approximate) openblack keeps the walk's goal in metres
glm::vec2 GoalOf(const map_coords::MapCoords& pos)
{
	return {map_coords::ToMetres(pos.x), map_coords::ToMetres(pos.z)};
}

/// villager::SetupMobileMoveToPos with the move-to state (12 LINEAR, 11 STEP_THROUGH)
void SetupMobileMoveToPos(entt::entity living, const map_coords::MapCoords& pos, uint8_t moveTo)
{
	villager::SetupMobileMoveToPos(
	    living, GoalOf(pos), moveTo == k_MoveToNextNode ? components::MoveState::StepThrough : components::MoveState::Linear);
}

/// Out of the occupants' list of its node (every entry of this living)
void LeaveNode(entt::entity living, LivingFootpath& fp)
{
	if (fp.footpath != entt::null && fp.node != footpaths::k_NoNode)
	{
		footpaths::RemoveOccupant(fp.footpath, fp.node, living);
	}
}

/// The walk's rules read the game's footpaths
class GameFootpathQueries final: public footpath_walk::FootpathQueries
{
public:
	int32_t NextNodeNear(entt::entity footpath, const map_coords::MapCoords& pos, footpaths::NodeId& node,
	                     map_coords::MapCoords& out, int32_t direction, float maxDistance) const override
	{
		return footpaths::NextNodeNear(footpath, pos, node, out, direction, maxDistance);
	}
	int32_t StepNextNode(entt::entity footpath, footpaths::NodeId& node, map_coords::MapCoords& out,
	                     int32_t direction) const override
	{
		return footpaths::StepNextNode(footpath, node, out, direction);
	}
	[[nodiscard]] footpaths::NodeId HeadNode(entt::entity footpath) const override { return footpaths::HeadNode(footpath); }
	[[nodiscard]] footpaths::NodeId TailNode(entt::entity footpath) const override { return footpaths::TailNode(footpath); }
	[[nodiscard]] map_coords::MapCoords NodeCoords(entt::entity footpath, footpaths::NodeId node) const override
	{
		return footpaths::NodeCoords(footpath, node);
	}
};

/// The walk's result once it is at its goal
constexpr uint32_t k_Arrived = 0xA;
} // namespace

map_coords::MapCoords PosOf(entt::entity object)
{
	const auto xz = town_queries::PosOf(object);
	return map_coords::MapCoords {xz.x, xz.y, 0.0f};
}

map_coords::MapCoords GetArrivePos(entt::entity object)
{
	if (Entities().AllOf<components::BigForest>(object))
	{
		// the door, as every multi-map fixed object's
		const auto door = abode_queries::GetDoorPos(object);
		return map_coords::MapCoords {door.x, door.y, 0.0f};
	}
	return ArrivePosOf(object);
}

map_coords::MapCoords ArrivePosOf(entt::entity target, entt::entity living)
{
	auto& registry = Entities();
	if (registry.AllOf<components::BigForest>(target))
	{
		return object::BigForestGetArrivePos(target, living);
	}
	if (registry.AllOf<components::Field>(target))
	{
		return fields::GetArrivePos(target);
	}
	if (registry.AllOf<components::FishFarm>(target))
	{
		return fish_farms::GetArrivePos(target);
	}
	if (registry.AllOf<components::WorshipSite>(target))
	{
		// its special point 9 when it has it, else its position
		if (const auto point = worship::site::GetSpecialPos(target, 9); point.has_value())
		{
			return map_coords::FromWorld(*point);
		}
		return PosOf(target);
	}
	if (registry.AnyOf<components::Abode, components::StoragePit>(target))
	{
		const auto* info = abode_villagers::InfoOf(target);
		const auto type = info != nullptr ? info->abodeType : AbodeType::General;
		switch (type)
		{
		case AbodeType::Creche:
		case AbodeType::Graveyard:
		case AbodeType::Wonder:
		case AbodeType::TownCentre:
			return PosOf(target);
		default:
		{
			// the door
			const auto door = abode_queries::GetArrivePos(target);
			return map_coords::MapCoords {door.x, door.y, 0.0f};
		}
		}
	}
	if (const auto* dance = registry.TryGet<const components::Dance>(target); dance != nullptr)
	{
		// the place it is danced about, half a cell along x
		auto at = dance->place;
		at.x += k_DanceArriveOffset;
		return at;
	}
	return PosOf(target); // any other thing: its position
}

uint32_t SetupMoveOnFootpath(entt::entity living, entt::entity footpath, int32_t direction, uint8_t final,
                             footpaths::NodeId node)
{
	auto& fp = FootpathOf(living);
	fp.next = entt::null;
	LeaveNode(living, fp); // the old node
	fp.footpath = footpath;
	fp.reverse = (direction & 1) != 0;
	if (node == footpaths::k_NoNode)
	{
		node = footpaths::GetNearestPos(footpath, PosOf(living), fp.reverse ? 1 : 0);
	}
	fp.node = node;
	if (node == footpaths::k_NoNode)
	{
		return 0;
	}
	footpaths::AddOccupant(footpath, node, living); // at the head
	SetupMobileMoveToPos(living, footpaths::NodeCoords(footpath, node), k_MoveToFirstNode);
	// SetCurrentAndDestinationState(29, final); its result 1 would set the clip, which the state change picks in
	// openblack
	return villager::SetCurrentAndDestinationState(living, VillagerStates::MoveOnPath, static_cast<VillagerStates>(final));
}

uint32_t SetupMoveToWithHug(entt::entity living, const map_coords::MapCoords& pos, uint8_t final)
{
	// (approximate) the goal in metres for the vec2 overload
	return villager::SetupMoveToWithHug(living, glm::vec2(map_coords::ToMetres(pos.x), map_coords::ToMetres(pos.z)),
	                                    static_cast<VillagerStates>(final));
}

void SetNextFootpath(entt::entity living, entt::entity footpath)
{
	FootpathOf(living).next = footpath;
}

uint32_t MoveOnFootpath(entt::entity living, uint32_t moveResult)
{
	// still on the way: the walk's own result
	if (moveResult != k_Arrived)
	{
		return moveResult;
	}
	auto& fp = FootpathOf(living);
	const GameFootpathQueries queries;
	const auto reached =
	    footpath_walk::ReachNode(queries, fp, PosOf(living), [](uint32_t n) { return game_random::GameRand(n); });
	// out of the node it was on (every entry), then into the new one, at the head
	if (reached.leftNode != footpaths::k_NoNode)
	{
		footpaths::RemoveEveryOccupant(reached.leftFootpath, reached.leftNode, living);
	}
	if (reached.joinsNode)
	{
		footpaths::AddOccupant(fp.footpath, fp.node, living);
	}
	if (reached.outcome == footpath_walk::NodeReached::Outcome::Finished)
	{
		// nothing left: the walk's final state
		villager::SetTopStateToFinal(living);
		return k_Arrived;
	}
	// a straight walk to the new goal, without hugging round obstacles
	SetupMobileMoveToPos(living, reached.goal, k_MoveToNextNode);
	return 1;
}

uint32_t ExitMoveOnPath(entt::entity living)
{
	auto& registry = Entities();
	if (auto* fp = registry.TryGet<LivingFootpath>(living); fp != nullptr && fp->node != footpaths::k_NoNode)
	{
		// out of its node (every entry), and no node; the footpath is kept
		footpaths::RemoveEveryOccupant(fp->footpath, fp->node, living);
		fp->node = footpaths::k_NoNode;
	}
	// the circle hug reset: no object hugged any more. (approximate) openblack's WallHugObjectReference
	registry.Remove<components::WallHugObjectReference>(living);
	return 1;
}

bool IsFollowingNode(entt::entity living, entt::entity footpath, footpaths::NodeId node)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const components::LivingAction>(living);
	const auto* fp = registry.TryGet<const LivingFootpath>(living);
	return action != nullptr && fp != nullptr &&
	       action->states[static_cast<size_t>(components::LivingAction::Index::Top)] ==
	           static_cast<uint8_t>(VillagerStates::MoveOnPath) &&
	       fp->footpath == footpath && fp->node == node;
}

bool GoesTowardHead(entt::entity living)
{
	const auto* fp = Entities().TryGet<const LivingFootpath>(living);
	return fp != nullptr && fp->reverse;
}

void RePlanHug(entt::entity living)
{
	// LINEAR, and the look ahead from where it stands with the step it has: no new aim, no arrival test, the goal kept
	villager::SetLinearAndSweep(living);
}

void MoveToNode(entt::entity living, entt::entity footpath, footpaths::NodeId node)
{
	auto& fp = FootpathOf(living);
	fp.footpath = footpath;
	fp.node = node;
	footpaths::AddOccupant(footpath, node, living); // at the head
	// the goal = the node's coords, the step aimed at it, LINEAR and the look ahead; no arrival test
	auto& registry = Entities();
	auto* wallHug = registry.TryGet<components::WallHug>(living);
	auto* transform = registry.TryGet<components::Transform>(living);
	if (wallHug == nullptr || transform == nullptr)
	{
		return;
	}
	wallHug->goal = GoalOf(footpaths::NodeCoords(footpath, node));
	villager::InitStepsXZ(*transform, *wallHug);
	villager::SetLinearAndSweep(living);
}
} // namespace openblack::ecs::living_footpath
