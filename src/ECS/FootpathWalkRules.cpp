/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FootpathWalkRules.h"

#include "Common/GUtilsDistance.h"

namespace openblack::ecs::footpath_walk
{
using footpaths::k_NoNode;

namespace
{
/// The coordinate moved by a draw of GameRand(k_GoalJitterRange) - k_GoalJitterHalf, as 32-bit integers
int32_t Jittered(int32_t coordinate, uint32_t draw)
{
	return static_cast<int32_t>(static_cast<uint32_t>(coordinate) + draw - static_cast<uint32_t>(k_GoalJitterHalf));
}
} // namespace

bool SameSpot(const map_coords::MapCoords& a, const map_coords::MapCoords& b)
{
	// altitudes neither below nor above each other: equal, or either one NaN
	return a.x == b.x && a.z == b.z && !(a.altitude < b.altitude) && !(a.altitude > b.altitude);
}

WalkToObject ChooseWalkToObject(const map_coords::MapCoords& me, const map_coords::MapCoords& objectArrive,
                                const map_coords::MapCoords& pos)
{
	return SameSpot(me, objectArrive) && !SameSpot(pos, me) ? WalkToObject::Direct : WalkToObject::ThroughObject;
}

NodeReached ReachNode(const FootpathQueries& queries, components::LivingFootpath& walk, const map_coords::MapCoords& pos,
                      const GameRandDraw& gameRand)
{
	NodeReached result;
	const int32_t direction = walk.reverse ? 1 : 0;
	map_coords::MapCoords out {};
	int32_t found = 0;
	if (walk.node == k_NoNode)
	{
		// no node: the nearest one close enough, then the next
		found = queries.NextNodeNear(walk.footpath, pos, walk.node, out, direction, k_JoinDistance);
	}
	else
	{
		// out of the node it was on, then the next one
		result.leftFootpath = walk.footpath;
		result.leftNode = walk.node;
		found = queries.StepNextNode(walk.footpath, walk.node, out, direction);
	}

	if (found == 1)
	{
		result.outcome = NodeReached::Outcome::NextNode;
		result.joinsNode = true;
		// a node beyond this one: the goal moves off the node, x first
		auto beyond = walk.node;
		map_coords::MapCoords unused {};
		if (queries.StepNextNode(walk.footpath, beyond, unused, direction) == 1)
		{
			out.x = Jittered(out.x, gameRand(k_GoalJitterRange));
			out.z = Jittered(out.z, gameRand(k_GoalJitterRange));
		}
		result.goal = out;
		return result;
	}

	// the end of this footpath: on to the next one, if any
	if (walk.next != entt::null)
	{
		walk.footpath = walk.next;
		const auto head = queries.HeadNode(walk.footpath);
		const auto tail = queries.TailNode(walk.footpath);
		if (head == k_NoNode)
		{
			// (openblack, guard) an empty footpath: the original reads a node that is not there. The walk ends, the
			// footpath to take next kept
			walk.node = k_NoNode;
			return result;
		}
		// toward the head when the head is strictly the farther end; the walk starts from the nearer end
		const float toTail = gutils::GetMetresDistanceSq(queries.NodeCoords(walk.footpath, tail), pos);
		const float toHead = gutils::GetMetresDistanceSq(queries.NodeCoords(walk.footpath, head), pos);
		walk.reverse = toHead > toTail;
		walk.node = walk.reverse ? tail : head;
		// one node in (that end, if there is no other), whose exact position is the goal
		static_cast<void>(queries.StepNextNode(walk.footpath, walk.node, out, walk.reverse ? 1 : 0));
		result.outcome = NodeReached::Outcome::NextFootpath;
		result.joinsNode = true;
		result.goal = queries.NodeCoords(walk.footpath, walk.node);
		walk.next = entt::null;
		return result;
	}

	// nothing left: the node is let go
	walk.node = k_NoNode;
	return result;
}

} // namespace openblack::ecs::footpath_walk
