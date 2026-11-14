/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The points that are only a place: the creature's drinking waypoints and the scripts' markers, each a record of its
// position alone, and the game's lists that hold them, newest first
// (docs/wiki/engine/saved-games.md#writing-the-world-file, the waypoints and the scripts' markers).

#include <cstdint>

#include <algorithm>
#include <optional>
#include <set>
#include <variant>
#include <vector>

#include <GameTailRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptTimer.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillagerLabel.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WriteObjectParts.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// A drinking waypoint: the place the land's script gave it, in whole map units with no height, and no place flags,
/// as it is made; a loaded one as its load kept it (nothing moves it)
std::optional<gamesave::SavedRecord> WriteWaypoint(WriteContext& context, entt::entity entity)
{
	if (auto kept = context.KeptRecordOf(entity, gamesave::WaypointRecord::k_Type); kept.has_value())
	{
		return kept;
	}
	const auto& waypoint = context.GetRegistry().Get<const components::DrinkWaypoint>(entity);
	gamesave::WaypointRecord record;
	record.position.position = {
	    .x = map_coords::ToFixed(waypoint.position.x), .z = map_coords::ToFixed(waypoint.position.z), .altitude = 0.0f};
	return record;
}

/// The thing's mark of a hidden name, which every script marker carries
constexpr uint8_t k_NameHidden = 0x40;

/// A script's marker: where it stands, its height over the land, its place's marks (held by its script and under its
/// control) and its hidden name
std::optional<gamesave::SavedRecord> WriteMarker(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	gamesave::ScriptMarkerRecord record;
	record.thing.flags = k_NameHidden;
	record.position.flags = PositionFlagsOf(registry, entity, 0);
	if (const auto* transform = registry.TryGet<const components::Transform>(entity); transform != nullptr)
	{
		const auto coords = map_coords::FromWorld(transform->position);
		record.position.position = {.x = coords.x, .z = coords.z, .altitude = coords.altitude};
	}
	return record;
}

/// A script's timer: one a load made as kept; one made in play a thing with a place it never sets (no place, no
/// height), its place's marks (held by its script and under its control) and its hidden name. Then the turn it was set on and
/// the turns it was set for
std::optional<gamesave::SavedRecord> WriteTimer(WriteContext& context, entt::entity entity)
{
	const auto& registry = context.GetRegistry();
	const auto& timer = registry.Get<const components::ScriptTimer>(entity);
	gamesave::ScriptTimerRecord record;
	if (auto kept = context.KeptRecordOf(entity, gamesave::ScriptTimerRecord::k_Type); kept.has_value())
	{
		// a loaded one: its thing's and place's parts as its load kept them
		record = std::get<gamesave::ScriptTimerRecord>(std::move(*kept));
	}
	else
	{
		record.thing.flags = registry.AllOf<components::NameHidden>(entity) ? k_NameHidden : 0;
		record.position.flags = PositionFlagsOf(registry, entity, 0);
	}
	record.words = {timer.timer.setTurn, static_cast<uint32_t>(timer.timer.turns)};
	return record;
}

/// A list's objects: the ones made since the load, the newest first by the order given (from 1), then the loaded list
/// without the objects gone since
template <typename Component, typename Order>
std::vector<gamesave::ObjectNumber> NewestFirstBy(WriteContext& context, GlobalList list, Order order)
{
	const auto& registry = context.GetRegistry();
	std::vector<gamesave::ObjectNumber> loaded;
	std::set<entt::entity> loadedEntities;
	if (const auto* first = context.KeptWorldRecord(1); first != nullptr)
	{
		if (const auto* game = std::get_if<gamesave::GameHead>(first); game != nullptr)
		{
			for (const auto number : game->lists.at(static_cast<size_t>(list)).numbers)
			{
				const auto target = context.TargetOf(number);
				if (target.has_value() && target->entity != entt::null)
				{
					if (!registry.Valid(target->entity))
					{
						continue;
					}
					loadedEntities.insert(target->entity);
				}
				loaded.push_back(number);
			}
		}
	}
	std::vector<std::pair<uint32_t, entt::entity>> made;
	registry.Each<const Component>([&](entt::entity entity, const Component& each) {
		if (!loadedEntities.contains(entity))
		{
			made.emplace_back(order(each), entity);
		}
	});
	std::ranges::sort(made, std::ranges::greater {}, &std::pair<uint32_t, entt::entity>::first);
	std::vector<gamesave::ObjectNumber> numbers;
	for (const auto& [stamp, entity] : made)
	{
		if (stamp == 0)
		{
			context.Fail("entity " + std::to_string(entt::to_integral(entity)) + " of the game's list " +
			             std::to_string(static_cast<size_t>(list)) + " has no order of its making");
			return {};
		}
		numbers.push_back(context.Reference(entity));
	}
	numbers.insert(numbers.end(), loaded.begin(), loaded.end());
	return numbers;
}
} // namespace

void openblack::ecs::saved_world_write::RegisterMarkerWriters(ClassWriters& writers)
{
	writers.Register(
	    gamesave::WaypointRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::DrinkWaypoint>(entity); },
	    WriteWaypoint);
	writers.Register(
	    gamesave::ScriptMarkerRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::ScriptMarker>(entity); },
	    WriteMarker);
	writers.Register(
	    gamesave::ScriptTimerRecord::k_Type,
	    [](const Registry& registry, entt::entity entity) { return registry.AllOf<components::ScriptTimer>(entity); },
	    WriteTimer);
	// the land's script adds each waypoint at the head of the game's list of waypoints, in the order it makes them
	writers.RegisterList(GlobalList::Waypoints, [](WriteContext& context) {
		return NewestFirstBy<components::DrinkWaypoint>(
		    context, GlobalList::Waypoints, [](const components::DrinkWaypoint& waypoint) { return waypoint.number + 1; });
	});
	// a script's marker joins the head of the game's list of things with a place as it is made
	writers.RegisterList(GlobalList::ThingsWithPosition, [](WriteContext& context) {
		return NewestFirstBy<components::ScriptMarker>(
		    context, GlobalList::ThingsWithPosition,
		    [](const components::ScriptMarker& marker) { return marker.creationStamp; });
	});
}
