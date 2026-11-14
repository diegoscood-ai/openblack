/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The writers of the footpaths, their nodes, the footpath links and the footpath finders: each record made from the entity, its
// values in the order the original's save writes them, every value openblack does not model taken from the record the load
// kept. A footpath's nodes are not entities: they are a source whose objects are the footpath's parts, each named by its node
// id. See docs/wiki/engine/saved-games.md#writing-the-world-file.

#include <algorithm>
#include <utility>
#include <variant>
#include <vector>

#include <TownRecords.h>

#include "ECS/Components/Footpath.h"
#include "ECS/Components/FootpathFinder.h"
#include "ECS/Registry.h"
#include "GameWriters.h"

using namespace openblack::ecs::saved_world_write;
using openblack::ecs::components::Footpath;
using openblack::ecs::components::FootpathFinder;
using openblack::ecs::components::FootpathLink;
using openblack::gamesave::FootpathFinderRecord;
using openblack::gamesave::FootpathLinkRecord;
using openblack::gamesave::FootpathNodeRecord;
using openblack::gamesave::FootpathRecord;
using openblack::gamesave::SavedRecord;

namespace
{
/// The record of the class the entity's load kept, or the record an object made in play starts with (all zero)
template <typename Record>
Record KeptOrNew(const WriteContext& context, entt::entity entity)
{
	if (const auto kept = context.KeptRecordOf(entity, Record::k_Type); kept.has_value())
	{
		return std::get<Record>(*kept);
	}
	return {};
}

/// The source key of a footpath's node: the footpath and the node's id there
SourceKey NodeKey(entt::entity footpath, int32_t node)
{
	return {.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, static_cast<uint32_t>(node))};
}

/// The nodes of every footpath, the footpaths in the game's list order (the newest first) and each one's nodes from
/// its head
std::vector<uint64_t> NodeKeys(const openblack::ecs::Registry& registry)
{
	std::vector<std::pair<uint32_t, entt::entity>> footpaths;
	registry.Each<const Footpath>([&footpaths](entt::entity entity, const Footpath& footpath) {
		footpaths.emplace_back(footpath.creationStamp, entity);
	});
	std::ranges::stable_sort(footpaths, [](const auto& a, const auto& b) { return a.first > b.first; });
	std::vector<uint64_t> keys;
	for (const auto& [stamp, entity] : footpaths)
	{
		for (const auto& node : registry.Get<const Footpath>(entity).nodes)
		{
			keys.push_back(NodeKey(entity, node.id).id);
		}
	}
	return keys;
}

/// A footpath: the thing's part kept, then its nodes from the head, each named by its id in the footpath, and whether
/// it is in use
std::optional<SavedRecord> WriteFootpath(WriteContext& context, entt::entity entity)
{
	const auto& footpath = context.GetRegistry().Get<Footpath>(entity);
	auto record = KeptOrNew<FootpathRecord>(context, entity);
	record.nodes.numbers.clear();
	for (const auto& node : footpath.nodes)
	{
		record.nodes.numbers.push_back(context.Reference(NodeKey(entity, node.id)));
	}
	// a word the load reads as true when it is not zero: the saved word stays while it says the same
	if ((record.active != 0) != footpath.active)
	{
		record.active = footpath.active ? 1 : 0;
	}
	return record;
}

/// A node of a footpath: the thing's part kept, then its place, its flags and the living things that follow it. The
/// load does not put the followers on the node, so a loaded node writes those its record had, without the ones gone
/// since (a living leaves its node's followers as it is deleted); a node made since writes the ones on it
std::optional<SavedRecord> WriteFootpathNode(WriteContext& context, uint64_t id)
{
	const auto [owner, index] = PartOf(id);
	const auto& registry = context.GetRegistry();
	const auto* footpath = registry.Valid(owner) ? registry.TryGet<Footpath>(owner) : nullptr;
	if (footpath == nullptr)
	{
		return std::nullopt;
	}
	const auto node = std::ranges::find(footpath->nodes, static_cast<int32_t>(index), &Footpath::Node::id);
	if (node == footpath->nodes.end())
	{
		return std::nullopt;
	}
	FootpathNodeRecord record;
	if (const auto kept = context.KeptRecordOf(SourceKey {.type = FootpathNodeRecord::k_Type, .id = id}); kept.has_value())
	{
		const auto& saved = std::get<FootpathNodeRecord>(*kept);
		record.thing = saved.thing;
		record.followers = saved.followers;
		context.DropGone(record.followers);
	}
	else
	{
		for (const auto living : node->occupants)
		{
			record.followers.numbers.push_back(context.Reference(living));
		}
	}
	record.coords = {.x = node->coords.x, .z = node->coords.z, .altitude = node->coords.altitude};
	record.flags = node->flags;
	return record;
}

/// A footpath finder: the thing's part kept, then the two things it joins, its footpath and the nodes it plans from and
/// to (none once one has gone)
std::optional<SavedRecord> WriteFootpathFinder(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& finder = registry.Get<FootpathFinder>(entity);
	auto record = KeptOrNew<FootpathFinderRecord>(context, entity);
	const auto thing = [&registry, &context](entt::entity object) -> openblack::gamesave::ObjectNumber {
		return object != entt::null && registry.Valid(object) ? context.Reference(object) : 0;
	};
	record.from.number = thing(finder.from);
	record.to.number = thing(finder.to);
	const auto* footpath = finder.footpath != entt::null && registry.Valid(finder.footpath)
	                           ? registry.TryGet<const Footpath>(finder.footpath)
	                           : nullptr;
	record.footpath.number = footpath != nullptr ? context.Reference(finder.footpath, FootpathRecord::k_Type) : 0;
	const auto node = [&context, &finder, footpath](int32_t id) -> openblack::gamesave::ObjectNumber {
		const bool there =
		    footpath != nullptr && std::ranges::find(footpath->nodes, id, &Footpath::Node::id) != footpath->nodes.end();
		return there ? context.Reference(NodeKey(finder.footpath, id)) : 0;
	};
	record.startNode.number = node(finder.startNode);
	record.destNode.number = node(finder.destNode);
	return record;
}

/// A footpath link: the thing's part kept, then the footpaths it lists, in its order
std::optional<SavedRecord> WriteFootpathLink(WriteContext& context, entt::entity entity)
{
	const auto& link = context.GetRegistry().Get<FootpathLink>(entity);
	auto record = KeptOrNew<FootpathLinkRecord>(context, entity);
	record.footpaths.numbers.clear();
	for (const auto footpath : link.footpaths)
	{
		record.footpaths.numbers.push_back(context.Reference(static_cast<entt::entity>(footpath), FootpathRecord::k_Type));
	}
	return record;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterFootpathWriters(ClassWriters& writers)
{
	writers.Register(
	    FootpathRecord::k_Type, [](const Registry& registry, entt::entity entity) { return registry.AllOf<Footpath>(entity); },
	    WriteFootpath);
	writers.RegisterSource(FootpathNodeRecord::k_Type, NodeKeys, WriteFootpathNode);
	writers.Register(
	    FootpathFinderRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<FootpathFinder>(entity); },
	    WriteFootpathFinder);
	writers.Register(
	    FootpathLinkRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<FootpathLink>(entity); }, WriteFootpathLink);
}
