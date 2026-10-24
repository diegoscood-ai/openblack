/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingFootpath.h"

/// The rules of a Living's walk along a footpath, free of the registry: when a walk to an object takes the object's
/// footpaths, and what a Living walking a footpath (MOVE_ON_PATH) does each time it reaches its goal
namespace openblack::ecs::footpath_walk
{

/// With no node, the Living picks up the footpath only from a node at most this far away (metres)
inline constexpr float k_JoinDistance = 0.03f;
/// The goal on a node that is not the footpath's last is moved by GameRand(k_GoalJitterRange) - k_GoalJitterHalf map
/// units in x, then in z
inline constexpr uint32_t k_GoalJitterRange = 0x2000;
inline constexpr int32_t k_GoalJitterHalf = 0x1000;

/// The game's equality of two map positions: the same x and z, and altitudes that are not ordered apart (two NaN
/// altitudes, or a NaN and a number, count as equal; so do +0 and -0)
[[nodiscard]] bool SameSpot(const map_coords::MapCoords& a, const map_coords::MapCoords& b);

/// How a walk to an object's position starts
enum class WalkToObject : uint8_t
{
	Direct,        ///< a plain walk round obstacles to the position
	ThroughObject, ///< the object's own choice: its footpaths when it has a footpath link, else the plain walk
};

/// A Living standing on the object's arrive point and sent somewhere else walks there directly; any other walk to the
/// object goes through the object
[[nodiscard]] WalkToObject ChooseWalkToObject(const map_coords::MapCoords& me, const map_coords::MapCoords& objectArrive,
                                              const map_coords::MapCoords& pos);

/// The footpath queries the walk reads: the game's footpaths (ECS/Footpaths.h) or a test's fake
class FootpathQueries
{
public:
	virtual ~FootpathQueries() = default;
	/// footpaths::NextNodeNear
	virtual int32_t NextNodeNear(entt::entity footpath, const map_coords::MapCoords& pos, footpaths::NodeId& node,
	                             map_coords::MapCoords& out, int32_t direction, float maxDistance) const = 0;
	/// footpaths::StepNextNode
	virtual int32_t StepNextNode(entt::entity footpath, footpaths::NodeId& node, map_coords::MapCoords& out,
	                             int32_t direction) const = 0;
	/// footpaths::HeadNode / TailNode: the list's ends, hidden or not
	[[nodiscard]] virtual footpaths::NodeId HeadNode(entt::entity footpath) const = 0;
	[[nodiscard]] virtual footpaths::NodeId TailNode(entt::entity footpath) const = 0;
	/// footpaths::NodeCoords
	[[nodiscard]] virtual map_coords::MapCoords NodeCoords(entt::entity footpath, footpaths::NodeId node) const = 0;
};

/// The synced stream's GameRand(n)
using GameRandDraw = std::function<uint32_t(uint32_t)>;

/// What a Living walking a footpath does once its walk to the current goal has arrived
struct NodeReached
{
	enum class Outcome : uint8_t
	{
		NextNode,     ///< on to the next node of this footpath
		NextFootpath, ///< this footpath is done: on to the footpath it was to take next
		Finished,     ///< no node and no footpath left: the walk's final state comes next
	};
	Outcome outcome {Outcome::Finished};
	/// The node it was on, to be left (every entry of the Living in its occupants); k_NoNode for none
	entt::entity leftFootpath {entt::null};
	footpaths::NodeId leftNode {footpaths::k_NoNode};
	/// It goes into the occupants of the walk's new node, at the head
	bool joinsNode {false};
	/// NextNode, NextFootpath: the goal of the step-through walk to the node
	map_coords::MapCoords goal {};
};

/// The turn a Living on a footpath reaches its goal, with `walk` its footpath fields and `pos` where it stands:
/// - no node: the nearest node within k_JoinDistance and then the next one (footpaths::NextNodeNear);
/// - a node: it leaves the node, then the next one in its direction (footpaths::StepNextNode);
/// - a next node found: it joins it; when there is yet another node after it, two draws move the goal off the node
///   (k_GoalJitterRange), x first; NextNode;
/// - else, with a footpath to take next: that footpath becomes the walk's, its direction toward the head when its
///   head is strictly farther from `pos` than its tail; the walk starts at the tail (toward the head) or the head, one
///   node in (or at that end when there is no other); the goal is that node exactly, it joins it, no next footpath;
///   NextFootpath;
/// - else Finished, the node let go.
/// `walk` is updated as the Living's fields are
[[nodiscard]] NodeReached ReachNode(const FootpathQueries& queries, components::LivingFootpath& walk,
                                    const map_coords::MapCoords& pos, const GameRandDraw& gameRand);

} // namespace openblack::ecs::footpath_walk
