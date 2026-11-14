/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The footpaths, their nodes and the footpath links written from the entities the load made of them: each record as
// the load read it, a footpath made in play from its nodes, and the whole world file of a loaded game as it was
// (docs/wiki/engine/saved-games.md#writing-the-world-file)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <SavedRecords.h>
#include <TownRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/FootpathFinder.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
using gamesave::FootpathLinkRecord;
using gamesave::FootpathNodeRecord;
using gamesave::FootpathRecord;
using gamesave::ObjectNumber;
using gamesave::SavedRecord;
using gamesave::WaypointRecord;
namespace saved_world_load = openblack::ecs::saved_world_load;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// The frozen Land 1 autosave, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenLandOneAutosave()
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / "_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" /
	                  "Saved Games" / "20" / "20.sav";
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	return ReadAll(path);
}

/// The footpaths' writers alone: every other class, the game object too, is written from what its load kept
ClassWriters FootpathWriters()
{
	ClassWriters writers;
	RegisterFootpathWriters(writers);
	return writers;
}

/// The footpaths and links of a world file made by the load's own makers, and the objects of the file they became
struct LoadedFootpaths
{
	std::unordered_map<ObjectNumber, LoadedObject> loaded;
	size_t footpaths {0};
	size_t nodes {0};
	size_t links {0};
};

LoadedFootpaths LoadFootpaths(const gamesave::WorldRead& world)
{
	LoadedFootpaths made;
	const auto plan = saved_world_load::PlanOf(world);
	const saved_world_load::Makers makers {
	    .footpath =
	        [&made](const saved_world_load::PlannedFootpath& footpath) {
		        const auto entity = saved_world_load::MakeFootpathEntity(footpath);
		        made.loaded[footpath.number] = {.entity = entity, .type = FootpathRecord::k_Type};
		        ++made.footpaths;
		        // each node is a part of the footpath, named by its id there; the load makes them in the record's order
		        const auto& nodes = Locator::entitiesRegistry::value().Get<ecs::components::Footpath>(entity).nodes;
		        if (nodes.size() == footpath.record.nodes.numbers.size())
		        {
			        for (size_t i = 0; i < nodes.size(); ++i)
			        {
				        made.loaded[footpath.record.nodes.numbers[i]] = {
				            .type = FootpathNodeRecord::k_Type, .id = PartId(entity, static_cast<uint32_t>(nodes[i].id))};
				        ++made.nodes;
			        }
		        }
		        return entity;
	        },
	    .footpathLink =
	        [&made](const saved_world_load::PlannedFootpathLink& link, const std::vector<entt::entity>& footpaths) {
		        const auto entity = saved_world_load::MakeFootpathLinkEntity(link, footpaths);
		        made.loaded[link.number] = {.entity = entity, .type = FootpathLinkRecord::k_Type};
		        ++made.links;
		        return entity;
	        },
	    // the trees and the big forests are not made: their records are written as the save holds them
	    .tree = [](const saved_world_load::PlannedTree& /*tree*/, uint32_t /*forest*/) { return entt::entity {entt::null}; },
	    .bigForest = [](const saved_world_load::PlannedBigForest& /*big*/,
	                    uint32_t /*forest*/) { return entt::entity {entt::null}; },
	};
	saved_world_load::Apply(plan, makers);
	return made;
}

/// The record each loaded object of the three classes is written as, against the record the load read
void ExpectEachRecordAsRead(const gamesave::WorldRead& world, const LoadedFootpaths& made, const ClassWriters& writers,
                            const WorldSource& source)
{
	WriteContext context(writers, source);
	for (const auto& [number, object] : made.loaded)
	{
		std::optional<SavedRecord> written;
		if (object.id.has_value())
		{
			written = (*writers.SourceWriterOf(object.type))(context, *object.id);
		}
		else
		{
			written = (*writers.WriterOf(object.type))(context, object.entity);
		}
		ASSERT_TRUE(written.has_value()) << number;
		EXPECT_TRUE(*written == *world.objects.at(number - 1)) << number;
	}
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

/// A loaded game as its file reads: the game object listing a link and a footpath; the link lists the footpath, whose
/// first node has a follower (a waypoint stands in for it: the load makes no follower)
gamesave::WorldRead SyntheticWorld()
{
	gamesave::WorldRead world;
	world.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.lists[0].numbers = {2};
	game.lists[28].numbers = {3};
	world.objects.emplace_back(game);
	world.objects.emplace_back(FootpathLinkRecord {.thing = {.extra = 9, .flags = 1}, .footpaths = {.numbers = {3}}});
	world.objects.emplace_back(
	    FootpathRecord {.thing = {.destroyed = 0, .flags = 5}, .nodes = {.numbers = {4, 6}}, .active = 1});
	world.objects.emplace_back(FootpathNodeRecord {.thing = {.flags = 2},
	                                               .coords = {.x = 0x10000, .z = 0x20000, .altitude = 3.0f},
	                                               .flags = 1,
	                                               .followers = {.numbers = {5}}});
	WaypointRecord follower;
	follower.position.position = {.x = 0x50000, .z = 0x60000, .altitude = 1.0f};
	world.objects.emplace_back(follower);
	world.objects.emplace_back(FootpathNodeRecord {.coords = {.x = 0x30000, .z = 0x40000, .altitude = 0.5f}, .flags = 6});
	// the reads end inside out: the follower inside the first node, the nodes inside the footpath, it inside the link
	world.loadOrder = {5, 4, 6, 3, 2, 1};
	return world;
}
} // namespace

TEST(SavedWorldWriteFootpaths, EachFootpathNodeAndLinkTheLoadMadeIsWrittenAsItsRecordWasRead)
{
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto world = SyntheticWorld();
	const auto made = LoadFootpaths(world);
	ASSERT_EQ(made.footpaths, 1u);
	ASSERT_EQ(made.nodes, 2u);
	ASSERT_EQ(made.links, 1u);

	const auto writers = FootpathWriters();
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &world, .loaded = made.loaded};
	ExpectEachRecordAsRead(world, made, writers, source);

	// The whole file: the three classes by their writers, the follower from the kept world, as the file was
	const auto written = WriteWorld(writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(world));
	ASSERT_EQ(written.records.size(), 6u);
	for (const auto& record : written.records)
	{
		const bool kept = record.type == gamesave::k_GameType || record.type == WaypointRecord::k_Type;
		EXPECT_EQ(record.source, kept ? RecordSource::KeptWorld : RecordSource::Writer) << record.number;
	}
	// each node is its footpath's part, by its id there
	const auto footpath = made.loaded.at(3).entity;
	EXPECT_EQ(written.records[3].id, std::optional(PartId(footpath, 0)));
	EXPECT_EQ(written.records[5].id, std::optional(PartId(footpath, 1)));

	// a follower gone since the load has left its node, as a living leaves its node's followers as it is deleted
	auto loaded = made.loaded;
	const auto dead = registry.Create();
	loaded.try_emplace(5, LoadedObject {.entity = dead, .type = WaypointRecord::k_Type});
	registry.Destroy(dead);
	const WorldSource after {.registry = registry, .landscape = std::string(k_Land), .kept = &world, .loaded = loaded};
	WriteContext context(writers, after);
	const auto node = (*writers.SourceWriterOf(FootpathNodeRecord::k_Type))(context, PartId(footpath, 0));
	ASSERT_TRUE(node.has_value()) << context.Failure();
	EXPECT_TRUE(std::get<FootpathNodeRecord>(*node).followers.numbers.empty());
}

TEST(SavedWorldWriteFootpaths, AFootpathMadeInPlayIsWrittenFromItsNodesAndTheLivingsOnThem)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto footpath = ecs::footpaths::Create();
	auto& component = registry.Get<ecs::components::Footpath>(footpath);
	const map_coords::MapCoords head {.x = 0x11000, .z = 0x22000, .altitude = 2.0f};
	const map_coords::MapCoords tail {.x = 0x33000, .z = 0x44000, .altitude = 0.0f};
	component.nodes.push_back({.position = {}, .coords = head, .flags = 3, .id = 7, .occupants = {}});
	component.nodes.push_back({.position = {}, .coords = tail, .flags = 0, .id = 2, .occupants = {}});
	component.active = false;
	// a living on the tail node, of a class whose record its load kept
	const auto living = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<WaypointRecord>>(living, WaypointRecord {});
	component.nodes[1].occupants.push_back(living);
	const auto link = registry.Create();
	registry.Assign<ecs::components::FootpathLink>(link, glm::vec3(0.0f),
	                                               std::vector {static_cast<ecs::components::Footpath::Id>(footpath)});

	// a newer footpath, ahead of it in the game's list
	const auto newer = ecs::footpaths::Create();
	auto& newerComponent = registry.Get<ecs::components::Footpath>(newer);
	newerComponent.nodes.push_back({.position = {}, .coords = tail, .flags = 0, .id = 0, .occupants = {}});

	auto writers = GameWriters();
	EXPECT_FALSE(writers.RegisterSource(FootpathNodeRecord::k_Type, {}, {}));
	EXPECT_FALSE(writers.Register(FootpathNodeRecord::k_Type, {}, {}));
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	// the nodes in their footpaths' list order, each footpath's from its head
	EXPECT_EQ(context.Keys(FootpathNodeRecord::k_Type),
	          (std::vector {PartId(newer, 0), PartId(footpath, 7), PartId(footpath, 2)}));

	const auto linkRecord = (*writers.WriterOf(FootpathLinkRecord::k_Type))(context, link);
	ASSERT_TRUE(linkRecord.has_value());
	EXPECT_TRUE(*linkRecord == SavedRecord(FootpathLinkRecord {.footpaths = {.numbers = {context.Reference(footpath)}}}));

	const auto footpathRecord = (*writers.WriterOf(FootpathRecord::k_Type))(context, footpath);
	ASSERT_TRUE(footpathRecord.has_value());
	const auto headNumber = context.Reference({.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, 7)});
	const auto tailNumber = context.Reference({.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, 2)});
	EXPECT_NE(headNumber, tailNumber);
	EXPECT_TRUE(*footpathRecord == SavedRecord(FootpathRecord {.nodes = {.numbers = {headNumber, tailNumber}}, .active = 0}));

	const auto headRecord = context.RecordOf(headNumber);
	ASSERT_NE(headRecord.record, nullptr);
	EXPECT_EQ(headRecord.source, RecordSource::Writer);
	EXPECT_TRUE(*headRecord.record ==
	            SavedRecord(FootpathNodeRecord {.coords = {.x = 0x11000, .z = 0x22000, .altitude = 2.0f}, .flags = 3}));
	const auto tailRecord = context.RecordOf(tailNumber);
	ASSERT_NE(tailRecord.record, nullptr);
	EXPECT_TRUE(*tailRecord.record == SavedRecord(FootpathNodeRecord {.coords = {.x = 0x33000, .z = 0x44000, .altitude = 0.0f},
	                                                                  .flags = 0,
	                                                                  .followers = {.numbers = {context.Reference(living)}}}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	// A node the footpath no longer has stops the write
	EXPECT_EQ(context.RecordOf(context.Reference({.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, 5)})).record,
	          nullptr);
	EXPECT_FALSE(context.Failure().empty());
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldWriteFootpaths, TheFrozenLandOneAutosavesFootpathsAreWrittenAsReadAndTheFileAsItWas)
{
	const auto bytes = FrozenLandOneAutosave();
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto world = gamesave::ReadWorld(*bytes);
	const auto made = LoadFootpaths(world);
	// its 84 footpaths with their 792 nodes, and its 59 links
	ASSERT_EQ(made.footpaths, 84u);
	ASSERT_EQ(made.nodes, 792u);
	ASSERT_EQ(made.links, 59u);

	const auto writers = FootpathWriters();
	const WorldSource source {
	    .registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = made.loaded};
	ExpectEachRecordAsRead(world, made, writers, source);

	const auto written = WriteWorld(writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, *bytes);
	size_t byWriters = 0;
	for (const auto& record : written.records)
	{
		byWriters += record.source == RecordSource::Writer ? 1 : 0;
	}
	EXPECT_EQ(byWriters, 84u + 792u + 59u);
}

// A footpath finder made in play is written as the original saves one: the two things it joins (none once gone), its
// footpath, and the nodes it plans from and to, its footpath's head and last node as it was made
TEST(SavedWorldWriteFootpaths, AFootpathFinderIsWrittenWithItsEndsFootpathAndNodes)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto footpath = registry.Create();
	auto& path = registry.Assign<ecs::components::Footpath>(footpath);
	path.nodes.push_back({.id = 1});
	path.nodes.push_back({.id = 0});
	// a thing of a class the writers know for its far end; its near end gone
	const auto to = registry.Create();
	registry.Assign<ecs::components::FootpathLink>(to);
	const auto from = registry.Create();
	registry.Destroy(from);
	const auto entity = registry.Create();
	auto& finder = registry.Assign<ecs::components::FootpathFinder>(entity);
	finder.from = from;
	finder.to = to;
	finder.footpath = footpath;
	finder.startNode = 1;
	finder.destNode = 0;

	const auto writers = FootpathWriters();
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext context(writers, source);
	EXPECT_EQ(writers.ClaimsOf(registry, entity), std::vector<uint32_t> {gamesave::FootpathFinderRecord::k_Type});
	const auto written = (*writers.WriterOf(gamesave::FootpathFinderRecord::k_Type))(context, entity);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::FootpathFinderRecord>(*written);
	EXPECT_EQ(record.from.number, 0u);
	EXPECT_EQ(record.to.number, context.Reference(to));
	EXPECT_EQ(record.footpath.number, context.Reference(footpath, FootpathRecord::k_Type));
	EXPECT_EQ(record.startNode.number,
	          context.Reference(SourceKey {.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, 1)}));
	EXPECT_EQ(record.destNode.number,
	          context.Reference(SourceKey {.type = FootpathNodeRecord::k_Type, .id = PartId(footpath, 0)}));
	EXPECT_NE(record.startNode.number, record.destNode.number);
}
