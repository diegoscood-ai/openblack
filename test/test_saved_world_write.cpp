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

#include <array>
#include <bit>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <variant>
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
#include "ECS/SavedWorldLoad.h"

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
	    .frameCount = 4096,
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
	EXPECT_EQ(state->frameCount, 4096u);
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

TEST(SavedWorldWrite, TheScriptsCameraOwnerAndTheWideScreenGoWhereTheLoadReadsThem)
{
	gamesave::GameHead base;
	base.tail.scriptSecondWords = {1, 2, 3, 4, 5, 6, 0, 8};
	base.tail.helpLastWords.fill(9);
	const saved_game_save::GameValues values {
	    .cameraOwner = 12,
	    .wideScreen = saved_game_save::GameValues::WideScreen {.on = 1, .owner = 12},
	    .wideScreenTimer = 1500.0f,
	};
	const auto head = saved_game_save::HeadOf(values, base);
	// the seventh of the script's words and the sixth to eighth of the help's last words; the others stay
	EXPECT_EQ(head.tail.scriptSecondWords, (std::array<uint32_t, 8> {1, 2, 3, 4, 5, 6, 12, 8}));
	EXPECT_EQ(head.tail.helpLastWords[4], 9u);
	EXPECT_EQ(head.tail.helpLastWords[5], 1u);
	EXPECT_EQ(head.tail.helpLastWords[6], 12u);
	EXPECT_EQ(head.tail.helpLastWords[7], std::bit_cast<uint32_t>(1500.0f));
	EXPECT_EQ(head.tail.helpLastWords[8], 9u);

	gamesave::WorldRead world;
	world.objects.emplace_back(head);
	const auto state = saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->cameraOwner, 12u);
	EXPECT_EQ(state->wideScreen, 1u);
	EXPECT_EQ(state->wideScreenOwner, 12u);
	EXPECT_EQ(state->wideScreenTimer, 1500.0f);
	// a save right after the load writes them as read
	EXPECT_EQ(saved_game_save::HeadOf(saved_game_save::ValuesOf(*state), base).tail.scriptSecondWords,
	          head.tail.scriptSecondWords);
	EXPECT_EQ(saved_game_save::HeadOf(saved_game_save::ValuesOf(*state), base).tail.helpLastWords, head.tail.helpLastWords);
}

TEST(SavedWorldWrite, WhetherTheCreatureRoomShowsTheCreatureGoesJustBeforeTheSnow)
{
	gamesave::GameHead base;
	base.tail.cameraWords = {7, 8, 9, 1};
	const saved_game_save::GameValues values {.creatureInTemple = false};
	const auto head = saved_game_save::HeadOf(values, base);
	// the fourth of the words before the snow; the camera help's three stay
	EXPECT_EQ(head.tail.cameraWords, (std::array<uint32_t, 4> {7, 8, 9, 0}));

	gamesave::WorldRead world;
	world.objects.emplace_back(head);
	const auto state = saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->creatureInTemple, 0u);
	// a save right after the load writes it as read
	EXPECT_EQ(saved_game_save::HeadOf(saved_game_save::ValuesOf(*state), base).tail.cameraWords, head.tail.cameraWords);
	// shown again, the word is 1; any value but 0 a script left reads as shown
	EXPECT_EQ(saved_game_save::HeadOf({.creatureInTemple = true}, base).tail.cameraWords[3], 1u);
	auto other = head;
	other.tail.cameraWords[3] = 5;
	gamesave::WorldRead otherWorld;
	otherWorld.objects.emplace_back(other);
	EXPECT_TRUE(saved_game_save::ValuesOf(*saved_game_load::StateOf(otherWorld)).creatureInTemple.value_or(false));
}

// The audio's part of the game's tail as the original's audio save writes it after its list: the script music's type
// and whether it plays, the town the music last played for (0 for none, or for a town gone since) and the alignment
TEST(SavedWorldWrite, TheScriptMusicItsTownAndTheAlignmentGoWhereTheAudiosLoadReadsThem)
{
	gamesave::GameHead base;
	base.tail.audioWords = {3, 0};
	base.tail.audioWord = 7;
	// with no music the record keeps its values
	EXPECT_EQ(saved_game_save::HeadOf({}, base).tail.audioWords, base.tail.audioWords);
	EXPECT_EQ(saved_game_save::HeadOf({}, base).tail.audioWord, 7u);
	const saved_game_save::GameValues values {
	    .music = saved_game_save::GameValues::Music {.scriptType = 59, .scriptPlaying = 1, .alignment = -0.25f}};
	const auto head = saved_game_save::HeadOf(values, base);
	EXPECT_EQ(head.tail.audioWords, (std::array<uint32_t, 2> {59, 1}));
	EXPECT_EQ(head.tail.audioWord, std::bit_cast<uint32_t>(-0.25f));

	const auto kept = KeptWorld();
	ecs::Registry registry;
	const auto town = registry.Create();
	registry.Assign<FakeNode>(town, k_NodeAt);
	const auto gone = registry.Create();
	registry.Destroy(gone);
	const auto townOf = [](entt::entity entity) {
		return saved_game_save::GameValues {.music = saved_game_save::GameValues::Music {
		                                        .scriptType = 59, .scriptPlaying = 1, .town = entt::to_integral(entity)}};
	};
	for (const auto& [named, expectsTown] : {std::pair {town, true}, std::pair {gone, false}})
	{
		ClassWriters writers;
		RegisterNodeWriter(writers);
		RegisterGameObjectWriter(writers, [&townOf, named] { return townOf(named); });
		const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept};
		WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		const auto& game = std::get<gamesave::GameHead>(*written);
		EXPECT_EQ(game.tail.audioObject.number, expectsTown ? context.Reference(town) : 0U);
		EXPECT_EQ(game.tail.audioWords, (std::array<uint32_t, 2> {59, 1}));
	}
	// the load reads them back
	gamesave::WorldRead world;
	world.objects.emplace_back(head);
	const auto plan = ecs::saved_world_load::PlanOf(world);
	ASSERT_TRUE(plan.scriptMusic.has_value());
	EXPECT_EQ(plan.scriptMusic->type, 59u);
	EXPECT_EQ(plan.scriptMusic->playing, 1u);
}

// The help bubble's object and a bookmark's object, kept from the load, are none once gone since, as the original's help
// and bookmarks let go of a deleted object at their next turn; the bookmark keeps its place, and an object still there,
// or the help's other objects, keep their numbers
TEST(SavedWorldWrite, TheGamesHelpBubbleAndBookmarksNameNoObjectGoneSinceTheLoad)
{
	auto kept = KeptWorld();
	auto& keptGame = std::get<gamesave::GameHead>(*kept.objects[0]);
	keptGame.tail.helpObjects = {gamesave::Ref {2}, gamesave::Ref {3}, gamesave::Ref {3}};
	keptGame.tail.bookmarks.at(0).byte = 1;
	keptGame.tail.bookmarks.at(0).point = {1.0f, 2.0f, 3.0f};
	keptGame.tail.bookmarks.at(0).object = {3};
	keptGame.tail.bookmarks.at(1).byte = 1;
	keptGame.tail.bookmarks.at(1).object = {2};
	ecs::Registry registry;
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	ASSERT_TRUE(alive != entt::null);
	registry.Destroy(dead);
	const LoadedObjects loaded {{2, {.entity = alive, .type = FootpathLinkRecord::k_Type}},
	                            {3, {.entity = dead, .type = FootpathRecord::k_Type}}};
	ClassWriters writers;
	RegisterGameObjectWriter(writers, {});
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};
	WriteContext context(writers, source);
	const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& tail = std::get<gamesave::GameHead>(*written).tail;
	EXPECT_EQ(tail.helpObjects.at(0).number, 2u);
	EXPECT_EQ(tail.helpObjects.at(1).number, 0u);
	EXPECT_EQ(tail.helpObjects.at(2).number, 3u);
	EXPECT_EQ(tail.bookmarks.at(0).object.number, 0u);
	EXPECT_EQ(tail.bookmarks.at(0).byte, 1u);
	EXPECT_EQ(tail.bookmarks.at(0).point, keptGame.tail.bookmarks.at(0).point);
	EXPECT_EQ(tail.bookmarks.at(1).object.number, 2u);

	// the help bubble's object still there keeps its number
	keptGame.tail.helpObjects.at(1) = {2};
	WriteContext again(writers, source);
	const auto still = (*writers.WriterOf(gamesave::k_GameType))(again, entt::null);
	ASSERT_TRUE(still.has_value()) << again.Failure();
	EXPECT_EQ(std::get<gamesave::GameHead>(*still).tail.helpObjects.at(1).number, 2u);
}

// The scripts' record of the last thing hit and of what hit it, in the script's part of the game's tail: written as the
// physics keeps it now (0 for none, or a thing gone since); with no physics, as the load kept it but none once gone
// since, as the scripts' turn lets go of a thing no longer available. A load finds each one's entity again
TEST(SavedWorldWrite, TheScriptsHitRecordIsWrittenAsThePhysicsKeepsItAndReadBack)
{
	auto kept = KeptWorld();
	auto& keptGame = std::get<gamesave::GameHead>(*kept.objects[0]);
	keptGame.tail.scriptRefs = {gamesave::Ref {2}, gamesave::Ref {3}};
	ecs::Registry registry;
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	ASSERT_TRUE(alive != entt::null);
	registry.Destroy(dead);
	const LoadedObjects loaded {{2, {.entity = alive, .type = FootpathLinkRecord::k_Type}},
	                            {3, {.entity = dead, .type = FootpathRecord::k_Type}}};
	const WorldSource source {.registry = registry, .landscape = std::string(k_Land), .kept = &kept, .loaded = loaded};
	{
		ClassWriters writers;
		RegisterGameObjectWriter(writers, {});
		WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		const auto& refs = std::get<gamesave::GameHead>(*written).tail.scriptRefs;
		EXPECT_EQ(refs.at(0).number, 2u);
		EXPECT_EQ(refs.at(1).number, 0u);
	}
	const auto node = registry.Create();
	registry.Assign<FakeNode>(node, k_NodeAt);
	const auto gone = registry.Create();
	registry.Destroy(gone);
	for (const auto& [hit, hitter] : {std::pair {node, gone}, std::pair {entt::entity {entt::null}, node}})
	{
		ClassWriters writers;
		RegisterNodeWriter(writers);
		RegisterGameObjectWriter(writers, [hit, hitter] {
			const auto idOf = [](entt::entity entity) -> std::optional<uint32_t> {
				return entity != entt::null ? std::optional {entt::to_integral(entity)} : std::nullopt;
			};
			return saved_game_save::GameValues {
			    .hits = saved_game_save::GameValues::Hits {.hit = idOf(hit), .hitter = idOf(hitter)}};
		});
		WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::k_GameType))(context, entt::null);
		ASSERT_TRUE(written.has_value()) << context.Failure();
		const auto& refs = std::get<gamesave::GameHead>(*written).tail.scriptRefs;
		EXPECT_EQ(refs.at(0).number, hit == node ? context.Reference(node) : 0U);
		EXPECT_EQ(refs.at(1).number, hitter == node ? context.Reference(node) : 0U);
	}

	// the load: each number's entity, none for 0, for an object it did not make or for an entity gone
	gamesave::WorldRead world;
	world.objects.emplace_back(keptGame);
	EXPECT_EQ(LoadedScriptHits(registry, world, loaded), (std::array<entt::entity, 2> {alive, entt::null}));
	std::get<gamesave::GameHead>(*world.objects[0]).tail.scriptRefs = {gamesave::Ref {0}, gamesave::Ref {9}};
	EXPECT_EQ(LoadedScriptHits(registry, world, loaded), (std::array<entt::entity, 2> {entt::null, entt::null}));
	EXPECT_EQ(LoadedScriptHits(registry, gamesave::WorldRead {}, loaded),
	          (std::array<entt::entity, 2> {entt::null, entt::null}));
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

// The music attached to things is written as the original's audio save writes its list: in the audio's part of the
// game's tail, head first, each info's record with its type, its thing, whether it is enabled, finished and started,
// whether it plays at a place of its own and that place's map coordinates as they were kept
TEST(SavedWorldWrite, TheMusicAttachedToThingsIsWrittenInTheAudiosListHeadFirst)
{
	const auto kept = KeptWorld();
	ecs::Registry registry;
	const auto first = registry.Create();
	registry.Assign<FakeNode>(first, k_NodeAt);
	const auto second = registry.Create();
	registry.Assign<FakeNode>(second, k_WaypointAt);
	const std::vector<audio::ThingMusicInfo> infos {
	    {.type = 71, .thing = static_cast<audio::ThingId>(entt::to_integral(second)), .key = 5},
	    {.type = 72,
	     .thing = static_cast<audio::ThingId>(entt::to_integral(first)),
	     .enabled = 0,
	     .finished = 1,
	     .started = 1,
	     .hasPlayPosition = 1,
	     .playCoords = {.x = 100 << 16, .z = 200 << 16, .altitude = 3.0f},
	     .key = 2},
	};
	ClassWriters writers;
	RegisterNodeWriter(writers);
	RegisterGameObjectWriter(writers, {});
	RegisterThingMusicWriters(writers, [&infos] { return infos; });
	const auto written = WriteWorld(writers, {.registry = registry, .landscape = std::string(k_Land), .kept = &kept});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// after the kept lists' objects, each info, then the thing it names
	auto game = std::get<gamesave::GameHead>(*kept.objects[0]);
	game.tail.sounds.numbers = {5, 7};
	gamesave::ThingMusicInfoRecord plain {.word = 71, .object = {6}, .words = {1, 0, 0, 0}};
	gamesave::ThingMusicInfoRecord placed {
	    .word = 72,
	    .object = {8},
	    .words = {0, 1, 1, 1},
	    .point = {std::bit_cast<float>(int32_t {100 << 16}), std::bit_cast<float>(int32_t {200 << 16}), 3.0f}};
	EXPECT_EQ(written.bytes, FileOf({{1, game},
	                                 {2, *kept.objects[1]},
	                                 {3, *kept.objects[2]},
	                                 {4, *kept.objects[3]},
	                                 {5, plain},
	                                 {6, FootpathNodeRecord {.coords = k_WaypointAt, .flags = 2}},
	                                 {7, placed},
	                                 {8, FootpathNodeRecord {.coords = k_NodeAt, .flags = 2}}}));
	ASSERT_GT(written.records.size(), 6u);
	EXPECT_EQ(written.records[4].id, std::optional<uint64_t> {5});
	EXPECT_EQ(written.records[6].id, std::optional<uint64_t> {2});
}

// A kept list of the loaded file's objects drops those whose entities are gone since, in its order; one no entity stands
// for, or one still there, stays; a kept object gone since is none
TEST(SavedWorldWrite, AKeptListDropsTheLoadedObjectsGoneSince)
{
	ecs::Registry registry;
	const auto alive = registry.Create();
	const auto dead = registry.Create();
	registry.Destroy(dead);
	const WorldSource source {.registry = registry,
	                          .loaded = {{2, {.entity = alive, .type = WaypointRecord::k_Type}},
	                                     {3, {.entity = dead, .type = WaypointRecord::k_Type}},
	                                     {4, {.type = WaypointRecord::k_Type, .id = 7}}}};
	const ClassWriters writers;
	const WriteContext context(writers, source);
	gamesave::RefList list {.numbers = {3, 2, 9, 4, 3}};
	context.DropGone(list);
	EXPECT_EQ(list.numbers, (std::vector<ObjectNumber> {2, 9, 4}));
	EXPECT_EQ(context.UnlessGone(3), 0u);
	EXPECT_EQ(context.UnlessGone(2), 2u);
	EXPECT_EQ(context.UnlessGone(0), 0u);
}

// A loaded object whose entity has changed class since (a tree become a dead tree, a node become something else) is
// gone: the original deleted that object and made one of the new class, so a kept record or list still naming the old
// number names an object that is gone. Its writer is never asked to write what it no longer claims; the write stops
// with a refusal, and a kept list drops it
TEST(SavedWorldWrite, ALoadedObjectWhoseEntityChangedClassSinceIsGone)
{
	ecs::Registry registry;
	const auto changed = registry.Create();
	registry.Assign<FakeFootpath>(changed, 1u, std::vector<entt::entity> {});
	const auto node = registry.Create();
	registry.Assign<FakeNode>(node, k_NodeAt);
	// the loaded node, named 2, has since become a footpath
	const WorldSource source {.registry = registry,
	                          .loaded = {{2, {.entity = changed, .type = FootpathNodeRecord::k_Type}},
	                                     {3, {.entity = node, .type = FootpathNodeRecord::k_Type}}}};
	ClassWriters writers;
	RegisterFootpathWriter(writers, false);
	RegisterNodeWriter(writers);
	EXPECT_FALSE(writers.StillOf(FootpathNodeRecord::k_Type, registry, changed));
	EXPECT_TRUE(writers.StillOf(FootpathNodeRecord::k_Type, registry, node));
	EXPECT_TRUE(writers.StillOf(WaypointRecord::k_Type, registry, changed));
	// an entity no writer claims is left to the class's writer, and a class registered with no claim (named only by its
	// class, through another object) is never refused
	EXPECT_TRUE(writers.StillOf(FootpathNodeRecord::k_Type, registry, registry.Create()));
	EXPECT_TRUE(writers.Register(WaypointRecord::k_Type, {}, {}));
	EXPECT_TRUE(writers.StillOf(WaypointRecord::k_Type, registry, changed));
	WriteContext context(writers, source);
	EXPECT_TRUE(context.IsGone(2));
	EXPECT_FALSE(context.IsGone(3));
	gamesave::RefList list {.numbers = {2, 3}};
	context.DropGone(list);
	EXPECT_EQ(list.numbers, (std::vector<ObjectNumber> {3}));
	const auto made = context.RecordOf(2);
	EXPECT_EQ(made.record, nullptr);
	EXPECT_NE(context.Failure().find("is gone: it is no longer of that class"), std::string::npos) << context.Failure();
	// the same entity of its new class is another object, with a number of its own
	EXPECT_NE(context.Reference(changed), 2u);
}

// A bonfire on the map is made again by the load as a mobile static of the bonfire's row, its player its player
// record's, its record kept whole (its own word among it), as the original's bonfire loads a rock's parts and its word;
// one of another row is not a bonfire and is left as read
TEST(SavedWorldWrite, ABonfireOnTheMapIsPlannedAsAMobileStaticWithItsPlayer)
{
	namespace load = openblack::ecs::saved_world_load;
	gamesave::WorldRead world;
	world.objects.emplace_back(gamesave::GameHead {});
	world.objects.emplace_back(gamesave::PlayerRecord {.playerNumber = 1});
	gamesave::BonfireRecord bonfire;
	bonfire.base.object.info = load::MobileStaticInfoRowOf(MobileStaticInfo::Bonfire);
	bonfire.object.number = 2;
	bonfire.word = 0x1234;
	world.objects.emplace_back(bonfire);
	auto rockRow = bonfire;
	rockRow.base.object.info = load::MobileStaticInfoRowOf(MobileStaticInfo::Rock);
	world.objects.emplace_back(rockRow);
	world.loadOrder = {1, 2, 3, 4};
	const auto plan = load::PlanOf(world);
	std::vector<load::PlannedMobileStatic> planned;
	for (const auto& object : plan.objects)
	{
		if (const auto* statics = std::get_if<load::PlannedMobileStatic>(&object); statics != nullptr)
		{
			planned.push_back(*statics);
		}
	}
	ASSERT_EQ(planned.size(), 1u);
	EXPECT_EQ(planned.front().number, 3u);
	EXPECT_EQ(planned.front().type, MobileStaticInfo::Bonfire);
	EXPECT_EQ(planned.front().player, PlayerNames::PLAYER_TWO);
	ASSERT_TRUE(std::holds_alternative<gamesave::BonfireRecord>(planned.front().record));
	EXPECT_EQ(std::get<gamesave::BonfireRecord>(planned.front().record).word, 0x1234u);
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
