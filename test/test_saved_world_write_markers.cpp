/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature's drinking waypoints and the scripts' markers written from their entities: each a place alone, and the
// game's lists that hold them newest first, as the original adds each at the head as it is made; a loaded waypoint made
// again and written back as read (docs/wiki/engine/saved-games.md#writing-the-world-file, the waypoints and the
// scripts' markers)

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/MapSimData.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/SavedWorldLoad.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
ClassWriters MarkerWriters()
{
	ClassWriters writers;
	RegisterMarkerWriters(writers);
	return writers;
}

entt::entity Waypoint(ecs::Registry& registry, glm::vec3 position, uint32_t number)
{
	const auto entity = registry.Create();
	registry.Assign<components::DrinkWaypoint>(entity, components::DrinkWaypoint {.position = position, .number = number});
	return entity;
}

entt::entity Marker(ecs::Registry& registry, glm::vec3 position, uint32_t stamp)
{
	const auto entity = registry.Create();
	registry.Assign<components::Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<components::ScriptMarker>(entity, components::ScriptMarker {.creationStamp = stamp});
	return entity;
}
} // namespace

// A waypoint is written at the place the land's script gave it, in whole map units with no height and no flags
TEST(SavedWorldWriteMarkers, AWaypointIsWrittenAtItsPlace)
{
	ecs::Registry registry;
	const auto writers = MarkerWriters();
	const auto waypoint = Waypoint(registry, glm::vec3(1234.5f, 17.0f, 987.25f), 0);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ(writers.ClaimsOf(registry, waypoint), std::vector {gamesave::WaypointRecord::k_Type});
	const auto written = (*writers.WriterOf(gamesave::WaypointRecord::k_Type))(context, waypoint);
	ASSERT_TRUE(written.has_value());
	const auto& record = std::get<gamesave::WaypointRecord>(*written);
	EXPECT_EQ(record.position.position.x, map_coords::ToFixed(1234.5f));
	EXPECT_EQ(record.position.position.z, map_coords::ToFixed(987.25f));
	EXPECT_EQ(record.position.position.altitude, 0.0f);
	EXPECT_EQ(record.position.flags, 0u);
}

// The waypoints run newest first by the order the land made them, the markers by the order they were made; a marker
// made with no stamp of its making stops the write
TEST(SavedWorldWriteMarkers, TheirListsRunNewestFirst)
{
	ecs::Registry registry;
	const auto writers = MarkerWriters();
	const auto firstWaypoint = Waypoint(registry, glm::vec3(10.0f, 0.0f, 10.0f), 0);
	const auto thirdWaypoint = Waypoint(registry, glm::vec3(30.0f, 0.0f, 30.0f), 2);
	const auto secondWaypoint = Waypoint(registry, glm::vec3(20.0f, 0.0f, 20.0f), 1);
	const auto secondMarker = Marker(registry, glm::vec3(5.0f, 0.0f, 5.0f), 2);
	const auto firstMarker = Marker(registry, glm::vec3(6.0f, 0.0f, 6.0f), 1);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ(
	    (*writers.ListWriterOf(GlobalList::Waypoints))(context),
	    (std::vector {context.Reference(thirdWaypoint), context.Reference(secondWaypoint), context.Reference(firstWaypoint)}));
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::ThingsWithPosition))(context),
	          (std::vector {context.Reference(secondMarker), context.Reference(firstMarker)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	static_cast<void>(Marker(registry, glm::vec3(7.0f, 0.0f, 7.0f), 0));
	WriteContext unstamped(writers, source);
	EXPECT_TRUE((*writers.ListWriterOf(GlobalList::ThingsWithPosition))(unstamped).empty());
	EXPECT_FALSE(unstamped.Failure().empty());
}

// Integration test: needs the original's own save of a new Land 1 game (local reference data beside the game's
// folder); skipped without it. Its 47 waypoints are written with no height and no place flags, and its 7 markers with
// their script's taking (the thing's 0x40) and the marks of being held by a script and under its control (0x600), as
// the writers give them
TEST(SavedWorldWriteMarkers, TheOriginalsNewGameWaypointsAndMarkersHoldWhatTheWritersGive)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		GTEST_SKIP() << "the original's save of a new Land 1 game is local reference data";
	}
	std::ifstream file(std::filesystem::path(game).parent_path() / "original_saves" / "land1_new_answer3" /
	                       "profile__e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" / "Saved Games" / "20" /
	                       "20.sav",
	                   std::ios::binary);
	if (!file)
	{
		GTEST_SKIP() << "no save of a new Land 1 game made by the original";
	}
	const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
	const auto world = gamesave::ReadWorld(bytes);
	size_t waypoints = 0;
	size_t markers = 0;
	for (const auto& object : world.objects)
	{
		if (!object.has_value())
		{
			continue;
		}
		if (const auto* waypoint = std::get_if<gamesave::WaypointRecord>(&*object); waypoint != nullptr)
		{
			++waypoints;
			EXPECT_EQ(waypoint->position.position.altitude, 0.0f);
			EXPECT_EQ(waypoint->position.flags, 0u);
			EXPECT_EQ(waypoint->thing.flags, 0u);
		}
		if (const auto* marker = std::get_if<gamesave::ScriptMarkerRecord>(&*object); marker != nullptr)
		{
			++markers;
			EXPECT_EQ(marker->position.flags, position_flags::k_InScript | position_flags::k_ControlledByScript);
			EXPECT_EQ(marker->thing.flags, 0x40u);
		}
	}
	EXPECT_EQ(waypoints, 47u);
	EXPECT_EQ(markers, 7u);
}

// The save's waypoints are made again from the game's list, its last first, so that each is numbered in the order the
// land made them, at its saved place; each is written back as it was read, and the list with a waypoint made since at
// its head, then the loaded ones as they were
TEST(SavedWorldWriteMarkers, ASavedWaypointIsMadeInItsOrderAndWrittenBackAsRead)
{
	namespace load = ecs::saved_world_load;
	test::RestoreService<Locator::entitiesRegistry> restore;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto saved = [](float x, float z) {
		gamesave::WaypointRecord record;
		record.position.position = {.x = map_coords::ToFixed(x), .z = map_coords::ToFixed(z), .altitude = 0.0f};
		return record;
	};
	gamesave::WorldRead world;
	gamesave::GameHead game;
	// the second made is at the list's head
	game.lists.at(static_cast<size_t>(GlobalList::Waypoints)).numbers = {3, 2};
	world.objects.emplace_back(game);
	world.objects.emplace_back(saved(1234.5f, 987.25f));
	world.objects.emplace_back(saved(20.0f, 30.0f));
	world.loadOrder = {2, 3};
	LoadedObjects loaded;
	const auto census = load::Apply(load::PlanOf(world), load::Makers {.waypoint = load::MakeWaypointEntity}, &loaded);
	ASSERT_TRUE(loaded.contains(2));
	ASSERT_TRUE(loaded.contains(3));
	const auto first = loaded.at(2).entity;
	const auto second = loaded.at(3).entity;
	EXPECT_EQ(registry.Get<const components::DrinkWaypoint>(first).number, 0u);
	EXPECT_EQ(registry.Get<const components::DrinkWaypoint>(second).number, 1u);
	const auto place =
	    map_coords::ToWorld(map_coords::MapCoords {.x = map_coords::ToFixed(20.0f), .z = map_coords::ToFixed(30.0f)});
	EXPECT_EQ(registry.Get<const components::DrinkWaypoint>(second).position, place);
	const auto line = std::ranges::find(census, gamesave::WaypointRecord::k_Type, &load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, 2u);

	const auto made = Waypoint(registry, glm::vec3(50.0f, 0.0f, 60.0f), 2);
	const auto writers = MarkerWriters();
	const WorldSource source {.registry = registry, .landscape = {}, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	for (const auto number : {2u, 3u})
	{
		const auto written = (*writers.WriterOf(gamesave::WaypointRecord::k_Type))(context, loaded.at(number).entity);
		ASSERT_TRUE(written.has_value());
		EXPECT_EQ(std::get<gamesave::WaypointRecord>(*written), std::get<gamesave::WaypointRecord>(*world.objects[number - 1]));
	}
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::Waypoints))(context),
	          (std::vector<gamesave::ObjectNumber> {context.Reference(made), 3, 2}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}
