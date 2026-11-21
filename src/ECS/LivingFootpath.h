/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Footpaths.h"

// How a Living walks a footpath, the Living side. The footpaths, their nodes and the nodes' occupants are
// ECS/Footpaths.h's; its UseFootpathIfNecessary and the reroute of RerouteFootpathsAroundObstacle call the functions
// below, and so does the MOVE_ON_PATH row of the villager state table. The rules of each turn on the footpath are
// ECS/FootpathWalkRules.h's

namespace openblack::ecs::components
{
/// The Living's footpath fields
struct LivingFootpath
{
	entt::entity next {entt::null}; ///< The footpath to take after this one (UseFootpathIfNecessary's)
	entt::entity footpath {entt::null};
	footpaths::NodeId node {footpaths::k_NoNode};
	bool reverse {false}; ///< The direction along the footpath (towards the head)
};
} // namespace openblack::ecs::components

namespace openblack::ecs::living_footpath
{
/// The move-to states the walk on a footpath gives SetupMobileMoveToPos: 12 on the first node and in the reroute, 11
/// between nodes
inline constexpr uint8_t k_MoveToFirstNode = 0xC;
inline constexpr uint8_t k_MoveToNextNode = 0xB;
/// How far along x from the place it is danced about a dance is arrived at: half a cell, in map units
inline constexpr int32_t k_DanceArriveOffset = 0x8000;

/// The object's position as MapCoords ((approximate) from the Transform; altitude 0)
[[nodiscard]] map_coords::MapCoords PosOf(entt::entity object);
/// MOVE_ON_PATH's turn, once the walk's step has been taken and gave `moveResult`. Anything but 0xA (at its goal) is
/// returned as it is. At its goal (footpath_walk::ReachNode, the synced stream's draws): out of the node it was on
/// (every entry); then
/// - on to the next node of the footpath, or to the next footpath from its nearer end: into the new node's occupants
///   (at the head), a step-through walk to the new goal (no hugging round obstacles), 1;
/// - nothing left: no node, then the final state (SetTopStateToFinal), 0xA
uint32_t MoveOnFootpath(entt::entity living, uint32_t moveResult);
/// MOVE_ON_PATH's exit: out of its node's occupants (every entry), no node, no object hugged any more; the footpath and
/// the next one are kept. Always 1
uint32_t ExitMoveOnPath(entt::entity living);
/// The object's arrive point as a walk to it reads it, whatever the Living: ArrivePosOf, except that a big forest's is
/// its door, as for every other multi-map fixed object (not the point nearest the Living). Altitude 0
[[nodiscard]] map_coords::MapCoords GetArrivePos(entt::entity object);
/// The thing's arrive position by its class (`living` only for the BigForest):
/// - the door: an abode, a storage pit and every other multi-map fixed object (abode_queries::GetArrivePos);
/// - the position: a creche, a graveyard, a wonder, a town centre, a field, a fish farm and any other thing;
/// - a worship site: its special point 9 when it has it, else its position;
/// - BigForest: object::BigForestGetArrivePos(forest, living);
/// - a dance: the place it is danced about, half a cell further along x (k_DanceArriveOffset), its altitude kept
[[nodiscard]] map_coords::MapCoords ArrivePosOf(entt::entity target, entt::entity living = entt::null);

/// No next footpath, the footpath set; out of the old node's occupants; the direction = direction & 1; no node ->
/// footpaths::GetNearestPos(this, direction); none -> 0; into the node's occupants (head); SetupMobileMoveToPos(node,
/// 12); SetCurrentAndDestinationState(29 MOVE_ON_PATH, final), which picks the clip. Returns that
uint32_t SetupMoveOnFootpath(entt::entity living, entt::entity footpath, int32_t direction, uint8_t final,
                             footpaths::NodeId node);
/// SetupMoveToWithHug with a MapCoords goal (UseFootpathIfNecessary's fallback)
uint32_t SetupMoveToWithHug(entt::entity living, const map_coords::MapCoords& pos, uint8_t final);
/// The footpath to take after this one (UseFootpathIfNecessary's path through the storage pit)
void SetNextFootpath(entt::entity living, entt::entity footpath);

// ---- for the footpath's rebuild and its reroute around an obstacle (PurgeFollowerList, ClearFromPreviousNode /
// ClearFromNextNode, ClearForDeletion, RerouteFootpathsAroundObstacle) -------------------------------------------------
/// PurgeFollowerList's test: the living is in state 29 MOVE_ON_PATH on `node` of `footpath`
[[nodiscard]] bool IsFollowingNode(entt::entity living, entt::entity footpath, footpaths::NodeId node);
/// It walks the footpath towards the list's head (footpaths::GetNextNode's direction != 0)
[[nodiscard]] bool GoesTowardHead(entt::entity living);
/// The walk becomes LINEAR and looks ahead from where it stands with the step it has (no new aim, no arrival test, the
/// goal kept). The footpath, the node and state 29 are KEPT. The reroute calls it: ClearFromNextNode on the previous node for
/// the followers with GoesTowardHead, ClearFromPreviousNode on the current one for those without
void RePlanHug(entt::entity living);
/// ClearForDeletion's move: the node set, at the head of its occupants (footpaths::AddOccupant), the goal = its
/// coords, the step aimed at it (InitStepsXZ), LINEAR and the look ahead; no arrival test
void MoveToNode(entt::entity living, entt::entity footpath, footpaths::NodeId node);
} // namespace openblack::ecs::living_footpath
