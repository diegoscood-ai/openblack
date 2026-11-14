/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's global lists written from the entities of their kinds, newest first by their creation numbers, as the
// game adds each new object at the head of its list; a loaded game's list keeps its order behind the objects made
// since (docs/wiki/engine/saved-games.md#writing-the-world-file, the global lists)

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>
#include <cstring>

#include <array>
#include <bit>
#include <optional>
#include <string>
#include <vector>

#include <BuildingRecords.h>
#include <ForestRecords.h>
#include <SavedRecords.h>
#include <WorldListRecords.h>
#include <WorldReader.h>
#include <WorshipRecords.h>
#include <gtest/gtest.h>

#include "ECS/Components/Abode.h"
#include "ECS/Components/DrawListed.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TownArtefact.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/FireFlies.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorld/WriteObjectParts.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/TownStateSystemInterface.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::saved_world_write;
namespace components = openblack::ecs::components;

namespace
{
/// A class for the lanterns, so that the context numbers them
ClassWriters ListWriters()
{
	ClassWriters writers;
	writers.Register(
	    gamesave::FootpathRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::StreetLantern>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	RegisterGameListWriters(writers);
	return writers;
}

entt::entity Lantern(ecs::Registry& registry, uint32_t creationIndex)
{
	const auto entity = registry.Create();
	registry.Assign<components::StreetLantern>(entity);
	registry.Assign<components::ObjectCreationIndex>(entity, components::ObjectCreationIndex {.value = creationIndex});
	return entity;
}

std::vector<gamesave::ObjectNumber> LanternList(WriteContext& context, const ClassWriters& writers)
{
	return (*writers.ListWriterOf(GlobalList::StreetLanterns))(context);
}
} // namespace

// A new game's list holds every lantern, the newest first, whatever order they were made in the registry
TEST(SavedWorldWriteGameLists, ANewGamesListRunsNewestFirst)
{
	ecs::Registry registry;
	const auto old = Lantern(registry, 10);
	const auto newest = Lantern(registry, 30);
	const auto middle = Lantern(registry, 20);
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto numbers = LanternList(context, writers);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	EXPECT_EQ(numbers, (std::vector {context.Reference(newest), context.Reference(middle), context.Reference(old)}));
}

// A lantern with no creation number stops the write, as its place in the list is not known
TEST(SavedWorldWriteGameLists, AnObjectWithNoCreationNumberStopsTheWrite)
{
	ecs::Registry registry;
	Lantern(registry, 10);
	registry.Assign<components::StreetLantern>(registry.Create());
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_TRUE(LanternList(context, writers).empty());
	EXPECT_FALSE(context.Failure().empty());
}

// A loaded game's list keeps its loaded order, without the lanterns gone since, and the lanterns made since go in
// front of it, the newest first; the objects no entity stands for stay as loaded
TEST(SavedWorldWriteGameLists, ALoadedListKeepsItsOrderBehindTheObjectsMadeSince)
{
	ecs::Registry registry;
	const auto first = Lantern(registry, 5);
	const auto gone = Lantern(registry, 4);
	const auto second = Lantern(registry, 3);
	gamesave::WorldRead world;
	world.ResizeObjects(6);
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::StreetLanterns)).numbers = {2, 3, 6, 4};
	world.objects.at(0) = game;
	world.objects.at(5) = gamesave::FootpathRecord {};
	LoadedObjects loaded;
	loaded.try_emplace(2, LoadedObject {.entity = first, .type = gamesave::FootpathRecord::k_Type});
	loaded.try_emplace(3, LoadedObject {.entity = gone, .type = gamesave::FootpathRecord::k_Type});
	loaded.try_emplace(4, LoadedObject {.entity = second, .type = gamesave::FootpathRecord::k_Type});
	registry.Destroy(gone);
	const auto later = Lantern(registry, 40);
	const auto latest = Lantern(registry, 41);
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	const auto numbers = LanternList(context, writers);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	EXPECT_EQ(numbers, (std::vector<gamesave::ObjectNumber> {context.Reference(latest), context.Reference(later), 2, 6, 4}));
}

// A loaded object whose entity is of another class since (a lantern become something no lantern's writer claims, as a
// tree felled into a dead tree) has left the list, as the original deletes the object; its entity is no member now
TEST(SavedWorldWriteGameLists, ALoadedObjectOfAnotherClassSinceHasLeftTheList)
{
	ecs::Registry registry;
	const auto kept = Lantern(registry, 5);
	const auto changed = Lantern(registry, 4);
	gamesave::WorldRead world;
	world.ResizeObjects(3);
	gamesave::GameHead game;
	game.lists.at(static_cast<size_t>(GlobalList::StreetLanterns)).numbers = {2, 3};
	world.objects.at(0) = game;
	LoadedObjects loaded;
	loaded.try_emplace(2, LoadedObject {.entity = kept, .type = gamesave::FootpathRecord::k_Type});
	loaded.try_emplace(3, LoadedObject {.entity = changed, .type = gamesave::FootpathRecord::k_Type});
	registry.Remove<components::StreetLantern>(changed);
	registry.Assign<components::Tree>(changed);
	auto writers = ListWriters();
	writers.Register(
	    gamesave::TreeRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::Tree>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	const WorldSource source {.registry = registry, .kept = &world, .loaded = loaded};
	WriteContext context(writers, source);
	const auto numbers = LanternList(context, writers);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	EXPECT_EQ(numbers, (std::vector<gamesave::ObjectNumber> {2}));
}

// The lists openblack makes no objects for are empty in a new game
TEST(SavedWorldWriteGameLists, TheListsOfClassesOpenblackDoesNotMakeAreEmpty)
{
	ecs::Registry registry;
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	for (const auto list : {GlobalList::StreetLights, GlobalList::Waterfalls, GlobalList::Objects})
	{
		ASSERT_NE(writers.ListWriterOf(list), nullptr);
		EXPECT_TRUE((*writers.ListWriterOf(list))(context).empty());
	}
}

// The footpaths, the towns and the footpath finders, which are not objects, run in the order their kinds were made,
// the newest first; one made with no stamp of that order stops the write
TEST(SavedWorldWriteGameLists, TheFootpathsListRunsByTheOrderTheyWereMade)
{
	ecs::Registry registry;
	ClassWriters writers;
	writers.Register(
	    gamesave::FootpathRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::Footpath>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	RegisterGameListWriters(writers);
	const auto footpath = [&registry](uint32_t stamp) {
		const auto entity = registry.Create();
		registry.Assign<components::Footpath>(entity).creationStamp = stamp;
		return entity;
	};
	const auto first = footpath(1);
	const auto third = footpath(3);
	const auto second = footpath(2);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::Footpaths))(context),
	          (std::vector {context.Reference(third), context.Reference(second), context.Reference(first)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	footpath(0);
	WriteContext unstamped(writers, source);
	EXPECT_TRUE((*writers.ListWriterOf(GlobalList::Footpaths))(unstamped).empty());
	EXPECT_FALSE(unstamped.Failure().empty());
}

// The banks of mist, which are not objects, run in the order they were made, the newest first, as the game adds each
// at the head of its list of mist; one made with no stamp of that order stops the write
TEST(SavedWorldWriteGameLists, TheMistListRunsByTheOrderTheBanksWereMade)
{
	ecs::Registry registry;
	ClassWriters writers;
	writers.Register(
	    gamesave::MistRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::Mist>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	RegisterGameListWriters(writers);
	const auto mist = [&registry](uint32_t stamp) {
		const auto entity = registry.Create();
		registry.Assign<components::Mist>(entity, components::Mist {.creationStamp = stamp});
		return entity;
	};
	const auto second = mist(2);
	const auto first = mist(1);
	const auto third = mist(3);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::Mist))(context),
	          (std::vector {context.Reference(third), context.Reference(second), context.Reference(first)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	mist(0);
	WriteContext unstamped(writers, source);
	EXPECT_TRUE((*writers.ListWriterOf(GlobalList::Mist))(unstamped).empty());
	EXPECT_FALSE(unstamped.Failure().empty());
}

// The streams, which are not objects, run in the order they were made, the newest first, as CREATE_STREAM adds each
// at the head of the game's list of streams; one made with no stamp of that order stops the write
TEST(SavedWorldWriteGameLists, TheStreamListRunsByTheOrderTheStreamsWereMade)
{
	ecs::Registry registry;
	ClassWriters writers;
	writers.Register(
	    gamesave::StreamRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::Stream>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	RegisterGameListWriters(writers);
	const auto stream = [&registry](uint32_t stamp) {
		const auto entity = registry.Create();
		registry.Assign<components::Stream>(entity, components::Stream {.id = 4, .creationStamp = stamp});
		return entity;
	};
	const auto first = stream(1);
	const auto third = stream(3);
	const auto second = stream(2);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::Streams))(context),
	          (std::vector {context.Reference(third), context.Reference(second), context.Reference(first)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();

	stream(0);
	WriteContext unstamped(writers, source);
	EXPECT_TRUE((*writers.ListWriterOf(GlobalList::Streams))(unstamped).empty());
	EXPECT_FALSE(unstamped.Failure().empty());
}

// The villagers without a town run as the town state keeps them, the head first: a villager joins at the head as it
// loses its town, whatever order the villagers were made in
TEST(SavedWorldWriteGameLists, TheVillagersWithoutATownRunAsTheTownStateKeepsThem)
{
	ASSERT_TRUE(Locator::townStateSystem::has_value());
	auto& vagrants = Locator::townStateSystem::value().Vagrants();
	vagrants.clear();
	ecs::Registry registry;
	ClassWriters writers;
	writers.Register(
	    gamesave::VillagerRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return registry.AllOf<components::Villager>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	RegisterGameListWriters(writers);
	const auto villager = [&registry]() {
		const auto entity = registry.Create();
		registry.Assign<components::Villager>(entity).town = entt::null;
		return entity;
	};
	const auto older = villager();
	static_cast<void>(villager());
	const auto newer = villager();
	vagrants = {older, newer};
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::VillagersWithoutTown))(context),
	          (std::vector {context.Reference(older), context.Reference(newer)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	vagrants.clear();
}

// The objects of several map cells, of every kind, run in one list, the newest first by their creation numbers; a
// tree, which is not one, is not in it
TEST(SavedWorldWriteGameLists, TheObjectsOfSeveralCellsRunInOneListNewestFirst)
{
	ecs::Registry registry;
	auto writers = ListWriters();
	writers.Register(
	    gamesave::AbodeRecord::k_Type,
	    [](const ecs::Registry& registry, entt::entity entity) { return !registry.AllOf<components::StreetLantern>(entity); },
	    [](WriteContext& /*context*/, entt::entity /*entity*/) -> std::optional<gamesave::SavedRecord> {
		    return std::nullopt;
	    });
	const auto made = [&registry](uint32_t creationIndex) {
		const auto entity = registry.Create();
		registry.Assign<components::ObjectCreationIndex>(entity, components::ObjectCreationIndex {.value = creationIndex});
		return entity;
	};
	const auto abode = made(10);
	registry.Assign<components::Abode>(abode);
	const auto rock = made(30);
	registry.Assign<components::MobileStatic>(rock);
	const auto feature = made(20);
	registry.Assign<components::Feature>(feature);
	const auto tree = made(40);
	registry.Assign<components::Tree>(tree);
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::MultiMapFixed))(context),
	          (std::vector {context.Reference(rock), context.Reference(feature), context.Reference(abode)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// Every artefact, in a town, a site or neither (the original's artefact stays on its object once made), is one of the
// game's towns' artifacts, the newest made first, named by its class
TEST(SavedWorldWriteGameLists, EveryArtefactIsInTheListNewestMadeFirst)
{
	ecs::Registry registry;
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry};
	const auto older = registry.Create();
	registry.Assign<components::TownArtefact>(older).made = 1;
	const auto newer = registry.Create();
	auto& artefact = registry.Assign<components::TownArtefact>(newer);
	artefact.made = 2;
	artefact.inSite = true;
	WriteContext context(writers, source);
	EXPECT_EQ((*writers.ListWriterOf(GlobalList::TownArtifacts))(context),
	          (std::vector {context.Reference(newer, gamesave::TownArtifactRecord::k_Type),
	                        context.Reference(older, gamesave::TownArtifactRecord::k_Type)}));
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// The fireflies: the list is empty while there are none; with some, it names each, the list's head first, and each is
// written as the original's record of one: a map object of the firefly's row at where it is and made where it was
// made, then what it is doing, its home, the light and its last turn's place as raw map coordinates, its drawn point,
// its clock, drift, progress, flight's length and speeds, its six phases and its flags, then the land's reward table
// and whether the next nightfall tops them up
TEST(SavedWorldWriteGameLists, TheFirefliesListNamesEachAndEachIsWrittenAsTheOriginalsRecord)
{
	ecs::Registry registry;
	ecs::saved_world_write::FireFlyWorld world;
	ClassWriters writers;
	RegisterFireFlyWriters(writers, [&world] { return world; });
	const WorldSource source {.registry = registry};
	{
		WriteContext context(writers, source);
		EXPECT_TRUE((*writers.ListWriterOf(GlobalList::Fireflies))(context).empty());
		EXPECT_TRUE(context.Failure().empty()) << context.Failure();
	}
	ecs::SavedFireFly hovering {.state = ecs::fire_flies::State::Hovering,
	                            .coords = {.x = 0x100000, .z = 0x200000, .altitude = 6.0f},
	                            .madeAt = {.x = 0x110000, .z = 0x210000, .altitude = 0.5f},
	                            .home = {.x = 0x120000, .z = 0x220000, .altitude = 0.25f},
	                            .light = {.x = 0x100000, .z = 0x200000, .altitude = 6.0f},
	                            .last = {.x = 0x100010, .z = 0x200020, .altitude = 5.75f},
	                            .drawn = {160.0f, 7.0f, 320.0f},
	                            .clock = 12.5f,
	                            .drift = 1.0f,
	                            .progress = 1.0f,
	                            .duration = 6.75f,
	                            .speedA = 0.9f,
	                            .speedB = 1.1f,
	                            .phases = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f},
	                            .flags = 0,
	                            .creationIndex = 3659};
	auto asleep = hovering;
	asleep.state = ecs::fire_flies::State::Asleep;
	asleep.flags = 1;
	asleep.creationIndex = 3658;
	world.fireFlies = {hovering, asleep};
	world.sums.assign(42, 0.0f);
	world.sums.back() = 26.0f;
	world.probabilities.assign(42, 0.0f);
	world.probabilities.front() = 20.0f;
	world.maySpawn = false;
	WriteContext context(writers, source);
	const auto list = (*writers.ListWriterOf(GlobalList::Fireflies))(context);
	EXPECT_EQ(list, (std::vector {context.Reference(SourceKey {.type = gamesave::FireFlyRecord::k_Type, .id = 3659}),
	                              context.Reference(SourceKey {.type = gamesave::FireFlyRecord::k_Type, .id = 3658})}));
	EXPECT_EQ(context.Keys(gamesave::FireFlyRecord::k_Type), (std::vector<uint64_t> {3659, 3658}));
	const auto written = (*writers.SourceWriterOf(gamesave::FireFlyRecord::k_Type))(context, 3659);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::FireFlyRecord>(*written);
	EXPECT_EQ(record.position.position, (gamesave::MapCoords {.x = 0x100000, .z = 0x200000, .altitude = 6.0f}));
	EXPECT_EQ(record.position.flags, 0x1);
	EXPECT_EQ(record.object.info, 2090u);
	EXPECT_EQ(record.object.coords, (gamesave::MapCoords {.x = 0x110000, .z = 0x210000, .altitude = 0.5f}));
	EXPECT_EQ(record.object.creationIndex, 3659u);
	EXPECT_EQ(record.word, 1u);
	EXPECT_EQ(record.points[0], (gamesave::Point {std::bit_cast<float>(0x120000), std::bit_cast<float>(0x220000), 0.25f}));
	EXPECT_EQ(record.points[1], (gamesave::Point {std::bit_cast<float>(0x100000), std::bit_cast<float>(0x200000), 6.0f}));
	EXPECT_EQ(record.points[2], (gamesave::Point {std::bit_cast<float>(0x100010), std::bit_cast<float>(0x200020), 5.75f}));
	EXPECT_EQ(record.points[3], (gamesave::Point {160.0f, 7.0f, 320.0f}));
	EXPECT_EQ(record.words, (std::array<uint32_t, 6> {std::bit_cast<uint32_t>(12.5f), std::bit_cast<uint32_t>(1.0f),
	                                                  std::bit_cast<uint32_t>(1.0f), std::bit_cast<uint32_t>(6.75f),
	                                                  std::bit_cast<uint32_t>(0.9f), std::bit_cast<uint32_t>(1.1f)}));
	EXPECT_EQ(record.floats, (std::array<float, 6> {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f}));
	EXPECT_EQ(record.byte, 0);
	ASSERT_EQ(record.first.values.size(), 42u);
	EXPECT_EQ(record.first.values.back(), std::bit_cast<uint32_t>(26.0f));
	ASSERT_EQ(record.second.values.size(), 42u);
	EXPECT_EQ(record.second.values.front(), std::bit_cast<uint32_t>(20.0f));
	EXPECT_EQ(record.flag, 0);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A firefly with its object in the map writes its place's flags from that object: the object draw list's listed mark
// when the list holds it, none when not (this test's object is in no map, so it has no map bit either)
TEST(SavedWorldWriteGameLists, AFireflysPlaceFlagsAreItsObjectsTheDrawListsMarkAmongThem)
{
	const test::RestoreService<Locator::mapCellsSystem> restoreCells;
	Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
	ecs::Registry registry;
	const auto listed = registry.Create();
	registry.Assign<ecs::components::DrawListed>(listed);
	const auto unlisted = registry.Create();
	ecs::saved_world_write::FireFlyWorld world;
	world.fireFlies = {{.creationIndex = 3659, .object = listed}, {.creationIndex = 3658, .object = unlisted}};
	ClassWriters writers;
	RegisterFireFlyWriters(writers, [&world] { return world; });
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto flagsOf = [&writers, &context](uint64_t id) {
		const auto written = (*writers.SourceWriterOf(gamesave::FireFlyRecord::k_Type))(context, id);
		return written.has_value() ? std::get<gamesave::FireFlyRecord>(*written).position.flags : uint16_t {0xFFFF};
	};
	EXPECT_EQ(flagsOf(3659), position_flags::k_DrawListed);
	EXPECT_EQ(flagsOf(3658), 0);
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}

// A new game's statistics: the player's number as its own word, then the figures in the game's order from the
// statistics' bytes; bytes that are not the game's whole figures stop the write
TEST(SavedWorldWriteGameLists, APlayersStatisticsAreItsFiguresInTheGamesOrder)
{
	ecs::Registry registry;
	// the figures after the player: 3 words, the towns' 10, 25 counts, two graphs, 11 results, 6 run words, 36 casts
	// and 3 last words
	constexpr size_t k_Bytes = 4 * (3 + 10 + 25 + 11 + 6 + 36 + 3) + 2 * sizeof(gamesave::StatsGraph);
	std::vector<uint8_t> figures(k_Bytes, 0);
	const uint32_t position = 2;
	std::memcpy(figures.data() + 4, &position, 4);
	const uint32_t births = 17;
	std::memcpy(figures.data() + 4 * 13, &births, 4);
	const uint32_t townsAtStart = 3;
	std::memcpy(figures.data() + k_Bytes - 4, &townsAtStart, 4);
	ClassWriters writers;
	RegisterGameStatsWriters(writers, [&figures](size_t /*player*/) { return figures; });
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto* writer = writers.SourceWriterOf(gamesave::k_GameStatsType);
	ASSERT_NE(writer, nullptr);
	const auto written = (*writer)(context, 1);
	ASSERT_TRUE(written.has_value()) << context.Failure();
	const auto& record = std::get<gamesave::GameStatsRecord>(*written);
	EXPECT_EQ(record.thing.extra, 1u);
	EXPECT_EQ(record.position, 2u);
	EXPECT_EQ(record.counts.at(0), 17u);
	EXPECT_EQ(record.last.at(2), 3u);

	figures.pop_back();
	WriteContext shorter(writers, source);
	EXPECT_FALSE((*writer)(shorter, 1).has_value());
	EXPECT_FALSE(shorter.Failure().empty());
}

// With no puzzle game made, the puzzle games' list is empty
TEST(SavedWorldWriteGameLists, ThePuzzleGamesListIsEmptyWithNone)
{
	ecs::Registry registry;
	const auto writers = ListWriters();
	const WorldSource source {.registry = registry};
	WriteContext context(writers, source);
	const auto* writer = writers.ListWriterOf(GlobalList::PuzzleGame);
	ASSERT_NE(writer, nullptr);
	EXPECT_TRUE((*writer)(context).empty());
	EXPECT_TRUE(context.Failure().empty()) << context.Failure();
}
