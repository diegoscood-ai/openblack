/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The world file written from the entities: each entity by its class's writer, or from the record its load kept, a
// loaded object no entity stands for from the kept world, the numbers given in the walk's order, and an entity with
// neither a writer nor a kept record stopping the write (docs/wiki/engine/saved-games.md#openblack)

#include <cstdint>

#include <bit>
#include <map>
#include <string>
#include <vector>

#include <ObjectStream.h>
#include <SavedRecords.h>
#include <TownRecords.h>
#include <VanillaSaveFile.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <gtest/gtest.h>

#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Player.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace saved_game_load = openblack::ecs::saved_game_load;
namespace saved_game_save = openblack::ecs::saved_game_save;
using gamesave::FootpathLinkRecord;
using gamesave::FootpathNodeRecord;
using gamesave::FootpathRecord;
using gamesave::ObjectNumber;
using gamesave::SavedRecord;
using gamesave::WaypointRecord;

namespace
{
constexpr std::string_view k_Land = ".\\DATA\\LANDSCAPE\\Land1.lnd";

/// The fake state a footpath writer reads: whether it is in use and its nodes
struct FakeFootpath
{
	uint32_t active {0};
	std::vector<entt::entity> nodes;
};

/// The fake state a footpath node writer reads
struct FakeNode
{
	gamesave::MapCoords coords;
};

/// A footpath's writer: its nodes by the numbers the context gives, its other values from its fake state, the part
/// every object writes from its kept record when it has one
void RegisterFootpathWriter(ClassWriters& writers, bool nodesByClass)
{
	writers.Register(
	    FootpathRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<FakeFootpath>(entity); },
	    [nodesByClass](WriteContext& context, entt::entity entity) -> std::optional<SavedRecord> {
		    const auto& footpath = context.GetRegistry().Get<FakeFootpath>(entity);
		    FootpathRecord record;
		    if (const auto kept = context.KeptRecordOf(entity, FootpathRecord::k_Type); kept.has_value())
		    {
			    record.thing = std::get<FootpathRecord>(*kept).thing;
		    }
		    for (const auto node : footpath.nodes)
		    {
			    record.nodes.numbers.push_back(nodesByClass ? context.Reference(node, FootpathNodeRecord::k_Type)
			                                                : context.Reference(node));
		    }
		    record.active = footpath.active;
		    return record;
	    });
}

void RegisterNodeWriter(ClassWriters& writers)
{
	writers.Register(
	    FootpathNodeRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<FakeNode>(entity); },
	    [](WriteContext& context, entt::entity entity) -> std::optional<SavedRecord> {
		    return FootpathNodeRecord {.coords = context.GetRegistry().Get<FakeNode>(entity).coords, .flags = 2};
	    });
}

constexpr gamesave::MapCoords k_NodeAt {.x = 0x10000, .z = 0x20000, .altitude = 3.0f};
constexpr gamesave::MapCoords k_WaypointAt {.x = 0x50000, .z = 0x60000, .altitude = 1.0f};

/// A loaded game as its load keeps it: the game object listing a footpath link, a footpath and a waypoint; the link
/// lists the footpath
gamesave::WorldRead KeptWorld()
{
	gamesave::WorldRead kept;
	kept.framing = *gamesave::ReadVanillaFraming(gamesave::WriteVanillaFraming(k_Land));
	gamesave::GameHead game;
	game.landNumber = 1;
	game.lists[0].numbers = {2, 3, 4};
	kept.objects.emplace_back(game);
	kept.objects.emplace_back(FootpathLinkRecord {.footpaths = {.numbers = {3}}});
	kept.objects.emplace_back(FootpathRecord {.thing = {.destroyed = 0, .flags = 5}});
	WaypointRecord waypoint;
	waypoint.position.position = k_WaypointAt;
	kept.objects.emplace_back(waypoint);
	return kept;
}

/// The bytes the game writes for the records given, by their numbers in the file
std::vector<uint8_t> FileOf(const std::map<ObjectNumber, SavedRecord>& records)
{
	auto bytes = gamesave::WriteVanillaFraming(k_Land);
	const auto framing = gamesave::ReadVanillaFraming(bytes);
	gamesave::StreamWriter writer({.objects = 1, .checksum = framing->checksum});
	gamesave::WriteNewObject writeNew;
	writeNew = [&](gamesave::StreamWriter& stream, ObjectNumber number) {
		gamesave::Write(stream, records.at(number), writeNew);
	};
	gamesave::Write(writer, records.at(1), writeNew);
	const auto body = writer.Take();
	bytes.insert(bytes.end(), body.begin(), body.end());
	return bytes;
}
} // namespace

TEST(SavedWorldWrite, WithNoWritersTheFileIsTheKeptWorlds)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	const ClassWriters writers;
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept});
	ASSERT_TRUE(written.Complete()) << written.failure;
	EXPECT_EQ(written.bytes, gamesave::WriteWorld(kept));
	ASSERT_EQ(written.records.size(), 4u);
	for (const auto& record : written.records)
	{
		EXPECT_EQ(record.source, RecordSource::KeptWorld);
	}
}

TEST(SavedWorldWrite, EachEntityIsWrittenByItsWriterOrFromItsKeptRecordInTheWalksOrder)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	// The link has no writer: its load kept its record. The footpath has one, and names a node made since the load,
	// which has one too
	const auto link = registry.Create();
	registry.Assign<ecs::components::LoadedRecord<FootpathLinkRecord>>(link, std::get<FootpathLinkRecord>(*kept.objects[1]));
	const auto node = registry.Create();
	registry.Assign<FakeNode>(node, k_NodeAt);
	const auto footpath = registry.Create();
	registry.Assign<FakeFootpath>(footpath, 1u, std::vector {node});
	registry.Assign<ecs::components::LoadedRecord<FootpathRecord>>(footpath, std::get<FootpathRecord>(*kept.objects[2]));
	ClassWriters writers;
	RegisterFootpathWriter(writers, false);
	RegisterNodeWriter(writers);
	EXPECT_FALSE(writers.Register(FootpathRecord::k_Type, {}, {}));

	const auto written = WriteWorld(writers, {.registry = registry,
	                                          .landscape = std::string(k_Land),
	                                          .kept = &kept,
	                                          .loaded = {{2, {.entity = link, .type = FootpathLinkRecord::k_Type}},
	                                                     {3, {.entity = footpath, .type = FootpathRecord::k_Type}}}});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// The file numbers each object at its first reference: the node, named by the footpath inside the link, is 4,
	// before the waypoint the game object names after the footpath
	const std::vector<WrittenRecord> order {
	    {.number = 1, .type = gamesave::k_GameType, .source = RecordSource::KeptWorld, .entity = entt::null},
	    {.number = 2, .type = FootpathLinkRecord::k_Type, .source = RecordSource::EntityRecord, .entity = link},
	    {.number = 3, .type = FootpathRecord::k_Type, .source = RecordSource::Writer, .entity = footpath},
	    {.number = 4, .type = FootpathNodeRecord::k_Type, .source = RecordSource::Writer, .entity = node},
	    {.number = 5, .type = WaypointRecord::k_Type, .source = RecordSource::KeptWorld, .entity = entt::null},
	};
	EXPECT_EQ(written.records, order);

	auto game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.lists[0].numbers = {2, 3, 5};
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, *kept.objects[1]},
	                                 {3, FootpathRecord {.thing = {.flags = 5}, .nodes = {.numbers = {4}}, .active = 1}},
	                                 {4, FootpathNodeRecord {.coords = k_NodeAt, .flags = 2}},
	                                 {5, *kept.objects[3]}}));
}

TEST(SavedWorldWrite, AnEntityWithNeitherAWriterNorAKeptRecordStopsTheWrite)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	const auto footpath = registry.Create();
	const auto madeInPlay = registry.Create();
	registry.Assign<FakeFootpath>(footpath, 1u, std::vector {madeInPlay});
	const WorldSource source {.registry = registry,
	                          .landscape = std::string(k_Land),
	                          .kept = &kept,
	                          .loaded = {{3, {.entity = footpath, .type = FootpathRecord::k_Type}}}};

	// Named by its class: no node writer, and its load kept nothing
	{
		ClassWriters writers;
		RegisterFootpathWriter(writers, true);
		const auto written = WriteWorld(writers, source);
		EXPECT_FALSE(written.Complete());
		EXPECT_NE(written.failure.find("has no writer and no record kept"), std::string::npos) << written.failure;
	}
	// Named alone: it is of no class at all
	{
		ClassWriters writers;
		RegisterFootpathWriter(writers, false);
		const auto written = WriteWorld(writers, source);
		EXPECT_FALSE(written.Complete());
		EXPECT_NE(written.failure.find("of no class"), std::string::npos) << written.failure;
	}
}

TEST(SavedWorldWrite, TheGameObjectsWriterStartsAFileWithNoKeptWorld)
{
	ecs::Registry registry;
	const auto node = registry.Create();
	registry.Assign<FakeNode>(node, k_NodeAt);
	ClassWriters writers;
	RegisterNodeWriter(writers);
	writers.Register(gamesave::k_GameType, {},
	                 [node](WriteContext& context, entt::entity entity) -> std::optional<SavedRecord> {
		                 EXPECT_TRUE(entity == entt::null);
		                 gamesave::GameHead game;
		                 game.lists[0].numbers = {context.Reference(node), context.Reference(node)};
		                 return game;
	                 });
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	ASSERT_TRUE(written.Complete()) << written.failure;
	gamesave::GameHead game;
	game.lists[0].numbers = {2, 2};
	EXPECT_EQ(written.bytes, FileOf({{1, game}, {2, FootpathNodeRecord {.coords = k_NodeAt, .flags = 2}}}));

	// With no writer of the game object and no kept world there is no file
	EXPECT_FALSE(WriteWorld(ClassWriters {}, {.registry = registry, .landscape = std::string(k_Land)}).Complete());
}

TEST(SavedWorldWrite, TheGameObjectsValuesGoWhereTheLoadReadsThem)
{
	gamesave::GameHead base;
	base.data[saved_game_load::k_RealGameTurn] = 41;
	base.words[saved_game_load::k_RenderTime] = 9;
	base.tail.snow.values.assign(4, 0);
	// with no values the record stays as it is
	EXPECT_EQ(saved_game_save::HeadOf({}, base), base);

	const saved_game_save::GameValues values {
	    .landNumber = 2,
	    .seeds = GameRandomSeeds {.synced = 0x1234, .local = 0x5678},
	    .gameTurn = 300,
	    .createdObjects = 77,
	    .influenceMultipliers = std::array {1.5f, 0.5f},
	    .clock = saved_game_save::GameValues::Clock {.visualTime = 13.0f, .cycle = {1700.0f, 0.07f, 0.083f}, .scale = 2.0f},
	    .snow =
	        ecs::systems::SavedSnow {
	            .depths = {1.0f, 2.0f, 3.0f, 4.0f}, .blockChanges = {0.5f}, .meltClock = 6.0f, .meltBand = 3},
	    .didYouKnow = std::array<std::vector<uint32_t>, 5> {std::vector<uint32_t> {4, 9}, {}, {1}, {}, {}},
	};
	gamesave::WorldRead world;
	world.objects.emplace_back(saved_game_save::HeadOf(values, base));
	const auto state = saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->landNumber, 2u);
	EXPECT_EQ(state->seeds.synced, 0x1234u);
	EXPECT_EQ(state->seeds.local, 0x5678u);
	EXPECT_EQ(state->gameTurn, 300u);
	EXPECT_EQ(state->createdObjects, 77u);
	EXPECT_EQ(state->playerInfluenceMultiplier, 1.5f);
	EXPECT_EQ(state->townInfluenceMultiplier, 0.5f);
	EXPECT_EQ(state->visualTime, 13.0f);
	// the cycle as the game keeps it: the day's length, the change, the night
	EXPECT_EQ(state->visualTimeCycle, (std::array {1700.0f, 0.07f, 0.083f}));
	EXPECT_EQ(state->visualTimeScale, 2.0f);
	EXPECT_EQ(state->snowDepths, (std::vector {1.0f, 2.0f, 3.0f, 4.0f}));
	EXPECT_EQ(state->snowBlockChanges, (std::vector {0.5f}));
	EXPECT_EQ(state->snowMeltClock, 6.0f);
	EXPECT_EQ(state->snowMeltBand, 3);
	EXPECT_EQ(state->didYouKnowTexts[0], (std::vector<uint32_t> {4, 9}));
	EXPECT_EQ(state->didYouKnowCounts[0], 2u);
	EXPECT_EQ(state->didYouKnowCounts[2], 1u);
	// the values openblack does not keep stay the record's
	EXPECT_EQ(state->realGameTurn, 41u);
	EXPECT_EQ(state->renderTime, 9u);

	// a grid of another size than the record's leaves the record's
	auto other = values;
	other.snow->depths = {1.0f};
	EXPECT_EQ(saved_game_save::HeadOf(other, base).tail.snow.values, base.tail.snow.values);
}

TEST(SavedWorldWrite, TheGameObjectsWriterKeepsTheLoadedRecordAndWritesItsListsByTheirWriters)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	const auto node = registry.Create();
	registry.Assign<FakeNode>(node, k_NodeAt);
	ClassWriters writers;
	RegisterNodeWriter(writers);
	RegisterGameObjectWriter(writers, [] { return saved_game_save::GameValues {.gameTurn = 77}; });
	EXPECT_TRUE(writers.RegisterList(GlobalList::Forests,
	                                 [node](WriteContext& context) { return std::vector {context.Reference(node)}; }));
	EXPECT_FALSE(writers.RegisterList(GlobalList::Forests, {}));
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// The lists in the game's order: the first as the load kept it, then the forests' by its writer
	auto game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.data[saved_game_load::k_GameTurn] = 77;
	game.lists[static_cast<size_t>(GlobalList::Forests)].numbers = {5};
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, *kept.objects[1]},
	                                 {3, *kept.objects[2]},
	                                 {4, *kept.objects[3]},
	                                 {5, FootpathNodeRecord {.coords = k_NodeAt, .flags = 2}}}));
	ASSERT_FALSE(written.records.empty());
	EXPECT_EQ(written.records.front().source, RecordSource::Writer);
	EXPECT_TRUE(GameWriters().Has(gamesave::k_GameType));
}

TEST(SavedWorldWrite, AGameNotLoadedNeedsEveryListsWriterAndItsPlayersWriter)
{
	ecs::Registry registry;
	const auto player = registry.Create();
	registry.Assign<ecs::components::Player>(player, PlayerNames::PLAYER_TWO);
	ClassWriters writers;
	RegisterGameObjectWriter(writers, {});
	{
		const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
		EXPECT_FALSE(written.Complete());
		EXPECT_NE(written.failure.find("global list 0 has no writer"), std::string::npos) << written.failure;
	}
	for (size_t list = 0; list < static_cast<size_t>(GlobalList::_COUNT); ++list)
	{
		writers.RegisterList(static_cast<GlobalList>(list),
		                     [](WriteContext& /*context*/) { return std::vector<ObjectNumber> {}; });
	}
	// The player is named in its place, and has no writer yet
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land)});
	EXPECT_FALSE(written.Complete());
	EXPECT_NE(written.failure.find("has no writer and no record kept"), std::string::npos) << written.failure;
}

TEST(SavedWorldWrite, ANonEntitySourcesObjectsAreNumberedAndWrittenInTheWalk)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	// The footpath (an entity with a writer) names two nodes of a source that is not entities, the second first
	constexpr uint64_t k_First = 10;
	constexpr uint64_t k_Second = 20;
	const auto footpath = registry.Create();
	ClassWriters writers;
	EXPECT_TRUE(writers.Register(
	    FootpathRecord::k_Type, {}, [](WriteContext& context, entt::entity /*entity*/) -> std::optional<SavedRecord> {
		    const auto keys = context.Keys(FootpathNodeRecord::k_Type);
		    FootpathRecord record {.thing = {.flags = 5}, .active = 1};
		    for (auto key = keys.rbegin(); key != keys.rend(); ++key)
		    {
			    record.nodes.numbers.push_back(context.Reference({.type = FootpathNodeRecord::k_Type, .id = *key}));
		    }
		    return record;
	    }));
	EXPECT_TRUE(writers.RegisterSource(
	    FootpathNodeRecord::k_Type,
	    [](const ecs::Registry& /*registry*/) {
		    return std::vector {k_First, k_Second};
	    },
	    [](WriteContext& /*context*/, uint64_t id) -> std::optional<SavedRecord> {
		    return FootpathNodeRecord {.coords = {.x = static_cast<int32_t>(id), .z = 0, .altitude = 0.0f}, .flags = 2};
	    }));
	// a class has one writer: an entity writer or a source
	EXPECT_FALSE(writers.RegisterSource(FootpathRecord::k_Type, {}, {}));
	EXPECT_FALSE(writers.Register(FootpathNodeRecord::k_Type, {}, {}));

	// The waypoint is a loaded source object whose class has no source yet: it comes from the kept world
	constexpr uint64_t k_Waypoint = 7;
	const WorldSource source {.registry = registry,
	                          .landscape = std::string(k_Land),
	                          .kept = &kept,
	                          .loaded = {{3, {.entity = footpath, .type = FootpathRecord::k_Type}},
	                                     {4, {.type = WaypointRecord::k_Type, .id = k_Waypoint}}}};
	const auto written = WriteWorld(writers, source);
	ASSERT_TRUE(written.Complete()) << written.failure;
	const std::vector<WrittenRecord> order {
	    {.number = 1, .type = gamesave::k_GameType, .source = RecordSource::KeptWorld},
	    {.number = 2, .type = FootpathLinkRecord::k_Type, .source = RecordSource::KeptWorld},
	    {.number = 3, .type = FootpathRecord::k_Type, .source = RecordSource::Writer, .entity = footpath},
	    {.number = 4, .type = FootpathNodeRecord::k_Type, .source = RecordSource::Writer, .id = k_Second},
	    {.number = 5, .type = FootpathNodeRecord::k_Type, .source = RecordSource::Writer, .id = k_First},
	    {.number = 6, .type = WaypointRecord::k_Type, .source = RecordSource::KeptWorld, .id = k_Waypoint},
	};
	EXPECT_EQ(written.records, order);
	auto game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.lists[0].numbers = {2, 3, 6};
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, *kept.objects[1]},
	                                 {3, FootpathRecord {.thing = {.flags = 5}, .nodes = {.numbers = {4, 5}}, .active = 1}},
	                                 {4, FootpathNodeRecord {.coords = {.x = static_cast<int32_t>(k_Second)}, .flags = 2}},
	                                 {5, FootpathNodeRecord {.coords = {.x = static_cast<int32_t>(k_First)}, .flags = 2}},
	                                 {6, *kept.objects[3]}}));

	// A source object made since the load whose class has no source stops the write
	ClassWriters missing;
	missing.Register(
	    FootpathRecord::k_Type, {}, [](WriteContext& context, entt::entity /*entity*/) -> std::optional<SavedRecord> {
		    return FootpathRecord {.nodes = {.numbers = {context.Reference({.type = FootpathNodeRecord::k_Type, .id = 99})}}};
	    });
	const auto stopped = WriteWorld(missing, source);
	EXPECT_FALSE(stopped.Complete());
	EXPECT_NE(stopped.failure.find("has no writer and no record kept"), std::string::npos) << stopped.failure;
}

TEST(SavedWorldWrite, APartOfAnEntityHasItsOwnSourceIdAndItsKeptRecord)
{
	ecs::Registry registry;
	const auto first = registry.Create();
	const auto second = registry.Create();
	// Each part's id gives back its owner and its number there, and no two parts share one
	EXPECT_EQ(PartOf(PartId(first, 0)), (Part {.owner = first, .index = 0}));
	EXPECT_EQ(PartOf(PartId(second, 0xFFFFFFFFU)), (Part {.owner = second, .index = 0xFFFFFFFFU}));
	EXPECT_NE(PartId(first, 1), PartId(second, 1));
	EXPECT_NE(PartId(first, 1), PartId(first, 2));

	// A loaded part finds the record the loaded file had at its number; a part made since, or a key whose class is
	// not its record's, finds none
	const auto kept = KeptWorld();
	const SourceKey loaded {.type = WaypointRecord::k_Type, .id = PartId(first, 3)};
	const SourceKey misnamed {.type = FootpathNodeRecord::k_Type, .id = PartId(first, 4)};
	const WorldSource source {
	    .registry = registry,
	    .landscape = std::string(k_Land),
	    .kept = &kept,
	    .loaded = {{4, {.type = loaded.type, .id = loaded.id}}, {3, {.type = misnamed.type, .id = misnamed.id}}}};
	const ClassWriters writers;
	WriteContext context(writers, source);
	const auto record = context.KeptRecordOf(loaded);
	ASSERT_TRUE(record.has_value());
	EXPECT_TRUE(*record == *kept.objects[3]);
	EXPECT_FALSE(context.KeptRecordOf(misnamed).has_value());
	const SourceKey made {.type = WaypointRecord::k_Type, .id = PartId(second, 3)};
	EXPECT_NE(context.Reference(made), 0U);
	EXPECT_FALSE(context.KeptRecordOf(made).has_value());

	// A game not loaded keeps no record
	const WorldSource fresh {.registry = registry, .landscape = std::string(k_Land)};
	WriteContext freshContext(writers, fresh);
	EXPECT_FALSE(freshContext.KeptRecordOf(loaded).has_value());
}
