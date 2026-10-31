/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a load sets first from the game object's record: the land's file, the turn, the seeds, the render clock, the
// influence multipliers and the time of day; then the world's forests, big forests and trees made again from
// their records (docs/wiki/engine/saved-games.md#what-a-load-does)

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <array>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

#include <LHVM.h>
#include <SavedRecords.h>
#include <WorldReader.h>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "3D/MapCoords.h"
#include "3D/SnowCover.h"
#include "ECS/Archetypes/AbodeArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LoadedRecord.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/SavedGameLoad.h"
#include "ECS/SavedGameSave.h"
#include "ECS/SavedPlayers.h"
#include "ECS/SavedWorld/GameWriters.h"
#include "ECS/SavedWorld/WorldWriter.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/Implementations/ObjectCreationIndexSystem.h"
#include "ECS/Systems/Implementations/SnowSystem.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Climate.h"
#include "ECS/Weather/Storms.h"
#include "ECS/Weather/WeatherThing.h"
#include "GameClock.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::saved_game_load::StateOf;
namespace saved_world_load = openblack::ecs::saved_world_load;
using StoragePitPiles = openblack::ecs::archetypes::AbodeArchetype::StoragePitPiles;

namespace
{
/// A record put in a world read, made by the save library: the record variant is not made in this file
/// (it weighs gigabytes in the compiler)
template <typename Record>
void Put(std::optional<gamesave::SavedRecord>& slot, const Record& record)
{
	gamesave::PutRecord(slot, record);
}

template <typename Record>
void Append(gamesave::WorldRead& world, const Record& record)
{
	Put(world.AppendObject(), record);
}

std::vector<uint8_t> ReadAll(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

/// A frozen profile's saved game, or nothing without the game's data
std::optional<std::vector<uint8_t>> FrozenSave(const std::filesystem::path& relative)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto path = std::filesystem::path(game) / "Profiles" / relative;
	std::error_code error;
	if (!std::filesystem::is_regular_file(path, error))
	{
		return std::nullopt;
	}
	return ReadAll(path);
}
} // namespace

TEST(SavedGameLoad, TheValuesComeFromTheGameObjectsDataAndItsWords)
{
	gamesave::WorldRead world;
	world.framing.landscape = ".\\DATA\\LANDSCAPE\\Land2.lnd";
	gamesave::GameHead game;
	game.landNumber = 2;
	game.data = {11, 22, 1500, 900, 4321, 0, 0, 0};
	game.bytes = {3, 0, 1};
	game.words = {50,
	              150000,
	              31,
	              std::bit_cast<uint32_t>(1.0f),
	              std::bit_cast<uint32_t>(0.5f),
	              std::bit_cast<uint32_t>(12.0f),
	              std::bit_cast<uint32_t>(1700.0f),
	              std::bit_cast<uint32_t>(0.07f),
	              std::bit_cast<uint32_t>(0.083f),
	              std::bit_cast<uint32_t>(2.0f)};
	Append(world, game);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->landscape, ".\\DATA\\LANDSCAPE\\Land2.lnd");
	EXPECT_EQ(state->landNumber, 2u);
	EXPECT_EQ(state->seeds.synced, 11u);
	EXPECT_EQ(state->seeds.local, 22u);
	EXPECT_EQ(state->gameTurn, 1500u);
	EXPECT_EQ(state->realGameTurn, 900u);
	EXPECT_EQ(state->createdObjects, 4321u);
	EXPECT_EQ(state->playerIndex, 3);
	EXPECT_EQ(state->renderTime, 150000u);
	EXPECT_EQ(state->renderTimeIncrement, 31u);
	EXPECT_EQ(state->townInfluenceMultiplier, 0.5f);
	EXPECT_EQ(state->visualTime, 12.0f);
	EXPECT_EQ(state->visualTimeCycle[1], 0.07f);
	EXPECT_EQ(state->visualTimeScale, 2.0f);
	// The last autosave's turn is the loaded turn, and the clock starts at the turn's time
	EXPECT_EQ(state->lastAutoSaveTurn, 1500u);
	EXPECT_EQ(state->clockMs, 150000u);
	EXPECT_FALSE(StateOf(gamesave::WorldRead {}).has_value());
}

TEST(SavedGameLoad, TheDidYouKnowTextsReadComeFromTheGameTailCategoryByCategory)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.tail.didYouKnow.at(0).values = {4143, 4150};
	game.tail.didYouKnowWords.at(0) = 2;
	game.tail.didYouKnow.at(4).values = {4200};
	game.tail.didYouKnowWords.at(4) = 1;
	Append(world, game);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->didYouKnowTexts.at(0), (std::vector<uint32_t> {4143, 4150}));
	EXPECT_EQ(state->didYouKnowCounts.at(0), 2u);
	EXPECT_TRUE(state->didYouKnowTexts.at(1).empty());
	EXPECT_EQ(state->didYouKnowCounts.at(1), 0u);
	EXPECT_EQ(state->didYouKnowTexts.at(4), (std::vector<uint32_t> {4200}));
	EXPECT_EQ(state->didYouKnowCounts.at(4), 1u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedGameLoad, TheFrozenLandOneAutosaveLoadsWithItsTurnSeedsAndTimeOfDay)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->landscape, std::string(".\\DATA\\LANDSCAPE\\Land1.lnd"));
	EXPECT_EQ(state->landNumber, 1u);
	EXPECT_EQ(state->gameTurn, 0u);
	EXPECT_EQ(state->seeds.synced, 978160173u);
	EXPECT_EQ(state->seeds.local, 3112260296u);
	EXPECT_EQ(state->visualTime, 12.0f);
	EXPECT_EQ(state->visualTimeCycle[0], 1700.0f);
	EXPECT_EQ(state->visualTimeScale, 1.0f);
	EXPECT_EQ(state->playerInfluenceMultiplier, 1.0f);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedGameLoad, AFrozenLandOneAutosaveLaterInTheGameLoadsWithItsTurnAndRenderClock)
{
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "15" / "1" / "15.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen profile";
	}
	const auto state = StateOf(gamesave::ReadWorld(*bytes));
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->gameTurn, 15264u);
	EXPECT_EQ(state->realGameTurn, 6001u);
	EXPECT_EQ(state->renderTime, 1526388u);
	EXPECT_EQ(state->renderTimeIncrement, 31u);
	EXPECT_EQ(state->lastAutoSaveTurn, 15264u);
	EXPECT_EQ(state->clockMs, 1526400u);
	EXPECT_NEAR(state->visualTime, 21.5194f, 1e-4f);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedGameLoad, TheFrozenLandOneAutosavesScriptsComeBackWithTheirTwoTasks)
{
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	const auto path = game == nullptr ? std::filesystem::path()
	                                  : std::filesystem::path(game) / "Profiles" /
	                                        "_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" / "Saved Games" /
	                                        "20" / "Script_20.sav";
	std::error_code error;
	if (game == nullptr || !std::filesystem::is_regular_file(path, error))
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	lhvm::LHVM vm;
	ASSERT_EQ(vm.RestoreState(path), EXIT_SUCCESS);
	// saved at the land's start, when the temple was built, with two tasks
	EXPECT_EQ(vm.GetTasks().size(), 2u);
}

namespace
{
/// A tree record of that row, at (x, 16) cells, in the forest of that number
gamesave::TreeRecord SavedTree(uint32_t info, int32_t x, gamesave::ObjectNumber forest)
{
	gamesave::TreeRecord tree;
	tree.base.position.position = {.x = x << 16, .z = 0x100000, .altitude = 0.0f};
	tree.base.object.info = info;
	tree.base.object.creationIndex = 40U + static_cast<uint32_t>(x);
	tree.base.object.scale = 0.5f;
	tree.base.object.heading = 1.25f;
	tree.firstValue = 77;
	tree.secondValue = 0.9f;
	tree.forest.number = forest;
	return tree;
}

/// A forest record with that id, the two values of the whole game and its scenic flag
gamesave::ForestRecord SavedForest(uint32_t id, uint32_t lastTreeTurn, uint32_t nextId, bool scenic)
{
	gamesave::ForestRecord forest;
	forest.container.position.position = {.x = 0x20000, .z = 0x30000, .altitude = 0.0f};
	forest.id = id;
	forest.isScenic = scenic ? 1U : 0U;
	forest.firstGameWord = lastTreeTurn;
	forest.secondGameWord = nextId;
	return forest;
}

/// A big forest record of that row, in the forest of that number, holding 300 wood
gamesave::BigForestRecord SavedBigForest(uint32_t info, gamesave::ObjectNumber forest)
{
	gamesave::BigForestRecord bigForest;
	bigForest.base.position.position = {.x = 0x50000, .z = 0x50000, .altitude = 0.0f};
	bigForest.base.object.info = info;
	bigForest.forest.number = forest;
	bigForest.word = std::bit_cast<uint32_t>(300.0f);
	return bigForest;
}
} // namespace

TEST(SavedWorldLoad, TheTreeRowsRunDownFromTheFirstTreeRow)
{
	EXPECT_EQ(saved_world_load::TreeTypeOfInfo(2115), TreeInfo::Beech);
	EXPECT_EQ(saved_world_load::TreeTypeOfInfo(2103), TreeInfo::Pine);
	EXPECT_EQ(saved_world_load::TreeTypeOfInfo(2093), TreeInfo::Burnt);
	EXPECT_FALSE(saved_world_load::TreeTypeOfInfo(2092).has_value());
	EXPECT_FALSE(saved_world_load::TreeTypeOfInfo(2116).has_value());
	EXPECT_FALSE(saved_world_load::TreeTypeOfInfo(0).has_value());
}

TEST(SavedWorldLoad, TheBigForestRowsRunDownFromTheFirstBigForestRow)
{
	EXPECT_EQ(saved_world_load::BigForestTypeOfInfo(369), BigForestInfo::Info1);
	EXPECT_EQ(saved_world_load::BigForestTypeOfInfo(366), BigForestInfo::Info4);
	EXPECT_FALSE(saved_world_load::BigForestTypeOfInfo(365).has_value());
	EXPECT_FALSE(saved_world_load::BigForestTypeOfInfo(370).has_value());
}

// The plan takes the trees and big forests in the order their reads ended, leaves out a held tree and one of a row
// that is no tree's, and the forests from the last of the game's list to its first
TEST(SavedWorldLoad, ThePlanTakesTheObjectsInTheOrderTheirReadsEndedAndTheForestsOldestFirst)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.lists[4].numbers = {3, 2};
	world.ResizeObjects(8);
	Put(world.objects[0], game);
	Put(world.objects[1], SavedForest(5, 100, 9, true));
	Put(world.objects[2], SavedForest(5, 100, 9, false));
	Put(world.objects[3], SavedTree(2115, 1, 2));
	Put(world.objects[4], SavedTree(2103, 2, 3));
	auto held = SavedTree(2110, 3, 0);
	held.base.position.flags = 1U << 2U;
	Put(world.objects[5], held);
	Put(world.objects[6], SavedTree(500, 4, 0));
	Put(world.objects[7], SavedBigForest(368, 3));
	world.loadOrder = {5, 8, 4, 2, 6, 7, 3, 1};

	const auto plan = saved_world_load::PlanOf(world);
	ASSERT_EQ(plan.objects.size(), 3u);
	ASSERT_TRUE(std::holds_alternative<saved_world_load::PlannedTree>(plan.objects[0]));
	EXPECT_EQ(std::get<saved_world_load::PlannedTree>(plan.objects[0]).number, 5u);
	EXPECT_EQ(std::get<saved_world_load::PlannedTree>(plan.objects[0]).type, TreeInfo::Pine);
	ASSERT_TRUE(std::holds_alternative<saved_world_load::PlannedBigForest>(plan.objects[1]));
	EXPECT_EQ(std::get<saved_world_load::PlannedBigForest>(plan.objects[1]).number, 8u);
	EXPECT_EQ(std::get<saved_world_load::PlannedBigForest>(plan.objects[1]).type, BigForestInfo::Info2);
	ASSERT_TRUE(std::holds_alternative<saved_world_load::PlannedTree>(plan.objects[2]));
	EXPECT_EQ(std::get<saved_world_load::PlannedTree>(plan.objects[2]).number, 4u);
	EXPECT_EQ(std::get<saved_world_load::PlannedTree>(plan.objects[2]).type, TreeInfo::Beech);
	ASSERT_EQ(plan.forests.size(), 2u);
	EXPECT_EQ(plan.forests[0].number, 2u);
	EXPECT_EQ(plan.forests[1].number, 3u);
	EXPECT_EQ(plan.lastTreeCreatedTurn, 100u);
	EXPECT_EQ(plan.nextForestId, 9u);
	EXPECT_EQ(plan.census.at(gamesave::TreeRecord::k_Type), 4u);
	EXPECT_EQ(plan.census.at(gamesave::ForestRecord::k_Type), 2u);
	EXPECT_EQ(plan.census.at(gamesave::BigForestRecord::k_Type), 1u);
}

// The forests keep their ids (two may share one), their scenic flag and their records, each under its own key;
// the forest list gets the save's next id and last planting turn; each object is made in its order, of the forest its
// record names, and a forest points at the big forest its record names
TEST(SavedWorldLoad, TheForestsAndObjectsAreMadeWithTheirSavedIdsAndForests)
{
	const test::RestoreService<Locator::forestSystem> restore;
	auto& forests = Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	saved_world_load::WorldPlan plan;
	auto withBigForest = SavedForest(5, 100, 9, true);
	withBigForest.bigForest.number = 7;
	plan.forests.push_back({.number = 2, .record = withBigForest});
	plan.forests.push_back({.number = 3, .record = SavedForest(5, 100, 9, false)});
	plan.objects.emplace_back(
	    saved_world_load::PlannedTree {.number = 4, .type = TreeInfo::Pine, .record = SavedTree(2103, 1, 2)});
	plan.objects.emplace_back(
	    saved_world_load::PlannedBigForest {.number = 7, .type = BigForestInfo::Info1, .record = SavedBigForest(369, 2)});
	plan.objects.emplace_back(
	    saved_world_load::PlannedTree {.number = 5, .type = TreeInfo::Beech, .record = SavedTree(2115, 2, 3)});
	plan.objects.emplace_back(
	    saved_world_load::PlannedTree {.number = 6, .type = TreeInfo::Oak, .record = SavedTree(2110, 3, 0)});
	plan.lastTreeCreatedTurn = 100;
	plan.nextForestId = 9;
	plan.census = {{gamesave::BigForestRecord::k_Type, 1},
	               {gamesave::ForestRecord::k_Type, 2},
	               {gamesave::TreeRecord::k_Type, 4},
	               {0x10, 1}};

	std::vector<std::pair<gamesave::ObjectNumber, uint32_t>> made;
	const saved_world_load::Makers makers {
	    .tree =
	        [&made](const saved_world_load::PlannedTree& tree, uint32_t forest) {
		        made.emplace_back(tree.number, forest);
		        // the last one is not made
		        return tree.number == 6 ? entt::entity {entt::null} : static_cast<entt::entity>(tree.number);
	        },
	    .bigForest =
	        [&made](const saved_world_load::PlannedBigForest& bigForest, uint32_t forest) {
		        made.emplace_back(bigForest.number, forest);
		        return static_cast<entt::entity>(bigForest.number);
	        },
	};
	const auto census = saved_world_load::Apply(plan, makers);

	// each forest under its own key, both with the saved id 5
	ASSERT_EQ(forests.All().size(), 2u);
	ASSERT_TRUE(forests.All().contains(1));
	ASSERT_TRUE(forests.All().contains(2));
	EXPECT_EQ(forests.All().at(1).id, 5u);
	EXPECT_EQ(forests.All().at(2).id, 5u);
	EXPECT_TRUE(forests.All().at(1).scenic);
	EXPECT_FALSE(forests.All().at(2).scenic);
	ASSERT_TRUE(forests.All().at(2).saved.has_value());
	EXPECT_EQ(forests.All().at(2).saved->id, 5u);
	EXPECT_EQ(forests.All().at(1).centre.x, 20.0f);
	EXPECT_EQ(forests.All().at(1).centre.z, 30.0f);
	// the first forest points at its big forest, the other at none
	EXPECT_EQ(forests.All().at(1).bigForest, static_cast<entt::entity>(7));
	EXPECT_EQ(forests.All().at(2).bigForest, entt::entity {entt::null});
	// the forest made last is the newest, and the one a script's id 5 names
	EXPECT_GT(forests.All().at(2).created, forests.All().at(1).created);
	EXPECT_EQ(ecs::ResolveForestId(5), 2u);
	EXPECT_EQ(forests.LastTreeCreatedTurn(), 100u);
	// the next forest made without an id takes the saved counter's
	EXPECT_EQ(forests.All().at(forests.Create(0, glm::vec3(0.0f))).id, 9u);
	const std::vector<std::pair<gamesave::ObjectNumber, uint32_t>> expected {{4, 1}, {7, 1}, {5, 2}, {6, 0}};
	EXPECT_EQ(made, expected);
	const saved_world_load::Census expectedCensus {{.type = 0x10, .saved = 1, .made = 0},
	                                               {.type = gamesave::BigForestRecord::k_Type, .saved = 1, .made = 1},
	                                               {.type = gamesave::ForestRecord::k_Type, .saved = 2, .made = 2},
	                                               {.type = gamesave::TreeRecord::k_Type, .saved = 4, .made = 2}};
	EXPECT_EQ(census, expectedCensus);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesTreesAndForestsAreMade)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	// every record read once, the game object's read ending last
	size_t records = 0;
	for (const auto& [type, count] : world.Census())
	{
		records += count;
	}
	ASSERT_EQ(world.loadOrder.size(), records);
	EXPECT_EQ(world.loadOrder.back(), 1u);
	const auto plan = saved_world_load::PlanOf(world);
	// every tree and big forest stands on the map; the 1,204 trees at a place of Land 1's script have the script's
	// type there, and the three big forests are Land 1's script's, of its first row
	std::map<TreeInfo, uint32_t> types;
	uint32_t trees = 0;
	std::vector<float> bigForestXs;
	for (const auto& object : plan.objects)
	{
		if (const auto* tree = std::get_if<saved_world_load::PlannedTree>(&object); tree != nullptr)
		{
			++trees;
			++types[tree->type];
		}
		else if (const auto* big = std::get_if<saved_world_load::PlannedBigForest>(&object); big != nullptr)
		{
			EXPECT_EQ(big->type, BigForestInfo::Info1);
			bigForestXs.push_back(static_cast<float>(big->record.base.position.position.x) / 6553.6f);
		}
	}
	ASSERT_EQ(trees, 1346u);
	ASSERT_EQ(bigForestXs.size(), 3u);
	std::ranges::sort(bigForestXs);
	EXPECT_NEAR(bigForestXs[0], 1696.36f, 0.01f);
	EXPECT_NEAR(bigForestXs[1], 1912.80f, 0.01f);
	EXPECT_NEAR(bigForestXs[2], 2156.58f, 0.01f);
	const std::map<TreeInfo, uint32_t> atTheScriptsPlaces {
	    {TreeInfo::Beech, 66}, {TreeInfo::Birch, 69},  {TreeInfo::Conifer, 98}, {TreeInfo::ConiferA, 179},
	    {TreeInfo::Oak, 32},   {TreeInfo::OakA, 9},    {TreeInfo::Olive, 9},    {TreeInfo::Palm, 90},
	    {TreeInfo::PalmA, 14}, {TreeInfo::PalmB, 162}, {TreeInfo::PalmC, 9},    {TreeInfo::Pine, 100},
	    {TreeInfo::Bush, 334}, {TreeInfo::BushA, 5},   {TreeInfo::BushB, 26},   {TreeInfo::Burnt, 2},
	};
	for (const auto& [type, count] : atTheScriptsPlaces)
	{
		EXPECT_GE(types[type], count) << static_cast<int>(type);
	}
	ASSERT_EQ(plan.forests.size(), 9u);
	// Land 1's six towns, each with its script's player and tribe
	const std::map<uint32_t, std::pair<uint8_t, Tribe>> scriptTowns {
	    {0, {0, Tribe::NORSE}},    {1, {7, Tribe::CELTIC}}, {2, {7, Tribe::NORSE}},
	    {3, {7, Tribe::JAPANESE}}, {4, {7, Tribe::AZTEC}},  {5, {7, Tribe::CELTIC}},
	};
	ASSERT_EQ(plan.towns.size(), scriptTowns.size());
	for (const auto& town : plan.towns)
	{
		ASSERT_TRUE(scriptTowns.contains(town.record.id)) << town.record.id;
		EXPECT_EQ(town.record.playerNumber, scriptTowns.at(town.record.id).first) << town.record.id;
		EXPECT_EQ(static_cast<Tribe>(town.record.tribe), scriptTowns.at(town.record.id).second) << town.record.id;
	}

	const test::RestoreService<Locator::forestSystem> restore;
	auto& forests = Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	uint32_t inAForest = 0;
	uint32_t bigForestsInAForest = 0;
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	uint32_t pitsWhole = 0;
	size_t townForests = 0;
	for (const auto& town : plan.towns)
	{
		townForests += town.record.forests.numbers.size();
	}
	const saved_world_load::Makers makers {
	    .town =
	        [&registry](const saved_world_load::PlannedTown& town) {
		        const auto entity = registry.Create();
		        registry.Assign<ecs::components::Town>(entity, ecs::components::Town {.id = town.record.id});
		        return entity;
	        },
	    .tree =
	        [&inAForest](const saved_world_load::PlannedTree& tree, uint32_t forest) {
		        inAForest += forest != 0 ? 1 : 0;
		        return static_cast<entt::entity>(tree.number);
	        },
	    .bigForest =
	        [&bigForestsInAForest](const saved_world_load::PlannedBigForest& bigForest, uint32_t forest) {
		        bigForestsInAForest += forest != 0 ? 1 : 0;
		        return static_cast<entt::entity>(bigForest.number);
	        },
	    .pile = [&registry](const saved_world_load::PlannedPile& /*pile*/) { return registry.Create(); },
	    .storagePit =
	        [&registry, &pitsWhole](const saved_world_load::PlannedStoragePit& /*pit*/, uint32_t townKey,
	                                const StoragePitPiles& piles) {
		        // each pit has its town and its six piles
		        pitsWhole += townKey != ecs::components::Abode::k_NoTown && piles.food != entt::null &&
		                             std::ranges::none_of(piles.wood, [](entt::entity pile) { return pile == entt::null; })
		                         ? 1
		                         : 0;
		        return registry.Create();
	        },
	};
	const auto census = saved_world_load::Apply(plan, makers);
	EXPECT_EQ(forests.All().size(), 9u);
	EXPECT_EQ(inAForest, 581u);
	// every town's forests, in its lists
	size_t listed = 0;
	for (const auto& [town, list] : forests.TownLists())
	{
		listed += list.size();
	}
	EXPECT_GT(townForests, 0u);
	EXPECT_EQ(listed, townForests);
	// Land 1's two storage pits, each of its town with its food pile and its five wood piles
	EXPECT_EQ(pitsWhole, 2u);
	// every big forest has its forest, and that forest points back at it
	EXPECT_EQ(bigForestsInAForest, 3u);
	EXPECT_EQ(std::ranges::count_if(forests.All(), [](const auto& entry) { return entry.second.bigForest != entt::null; }), 3);
	for (const auto type :
	     {gamesave::TreeRecord::k_Type, gamesave::BigForestRecord::k_Type, gamesave::TownRecord::k_Type,
	      gamesave::StoragePitRecord::k_Type, gamesave::WoodPileRecord::k_Type, gamesave::FoodPileRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, line->saved);
	}
}

// The towns come from the last of the game's town list to its first, and the players' lists give the order their
// towns are listed in
TEST(SavedWorldLoad, ThePlanTakesTheTownsOldestFirstAndThePlayersListsInOrder)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.players = {2, 3, 0, 0, 0, 0, 0, 0};
	game.lists[31].numbers = {5, 4, 6};
	world.ResizeObjects(6);
	Put(world.objects[0], game);
	gamesave::PlayerRecord first;
	first.towns = {6, 4};
	Put(world.objects[1], first);
	gamesave::PlayerRecord second;
	second.towns = {5};
	Put(world.objects[2], second);
	for (const gamesave::ObjectNumber number : {4U, 5U, 6U})
	{
		gamesave::TownRecord town;
		town.id = number * 10;
		world.objects[number - 1] = town;
	}
	world.loadOrder = {4, 2, 5, 3, 6, 1};

	const auto plan = saved_world_load::PlanOf(world);
	ASSERT_EQ(plan.towns.size(), 3u);
	EXPECT_EQ(plan.towns[0].number, 6u);
	EXPECT_EQ(plan.towns[1].number, 4u);
	EXPECT_EQ(plan.towns[2].number, 5u);
	const std::vector<gamesave::ObjectNumber> owners {6, 4, 5};
	EXPECT_EQ(plan.ownersTowns, owners);
}

// A loaded dance is made after every dance there is, of its row, at its saved place, clock, speeds, state and times,
// with its groups as their records say and each dancer given its place; one whose dancer was not made is not made
TEST(SavedWorldLoad, ADanceIsMadeWithItsGroupsAndItsDancersPlaces)
{
	// a dance whose dancer is not made says so in the game's log: a null one when no test before made it
	if (!spdlog::get("game"))
	{
		spdlog::create<spdlog::sinks::null_sink_mt>("game");
	}
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	const auto dancer = registry.Create();
	saved_world_load::PlannedDance planned {.number = 10};
	planned.record.type = 15;
	planned.record.group.position.position = {.x = 0x0A000000, .z = 0x0B000000, .altitude = 0.0f};
	planned.record.group.time = 12.0f;
	planned.record.group.length = 1;
	planned.record.group.speed = 0.8f;
	planned.record.group.lastSpeed = 1.0f;
	planned.record.group.denominator = 2;
	planned.record.group.dancers = 1;
	planned.record.group.proportionGroups.numbers = {12, 11};
	planned.record.words = {1, 900, 31935, 0, 0, 0};
	gamesave::DanceGroupRecord first;
	first.members.numbers = {20};
	first.words = {0, 1, 0, 3};
	std::ranges::copy(std::string_view("G1"), first.text.begin());
	gamesave::DanceGroupRecord second;
	second.other.number = 11;
	std::ranges::copy(std::string_view("G2"), second.text.begin());
	planned.groups = {{11, first}, {12, second}};
	const auto objectOf = [dancer](gamesave::ObjectNumber number) { return number == 20 ? dancer : entt::entity {entt::null}; };
	const auto entity = saved_world_load::MakeDanceEntity(planned, objectOf);
	ASSERT_TRUE(registry.Valid(entity));
	const auto& dance = registry.Get<ecs::components::Dance>(entity);
	EXPECT_EQ(dance.type, static_cast<DanceInfo>(15));
	EXPECT_EQ(dance.place.x, 0x0A000000);
	EXPECT_EQ(dance.clock, 12.0f);
	EXPECT_EQ(dance.state, ecs::components::Dance::State::Stopped);
	EXPECT_EQ(dance.startTurn, 31935u);
	EXPECT_EQ(dance.duration, 900u);
	ASSERT_EQ(dance.groups.all.size(), 2u);
	EXPECT_EQ(dance.groups.all[0].name, "G1");
	EXPECT_EQ(dance.groups.all[0].shape, 3u);
	EXPECT_EQ(dance.groups.all[0].dancers, std::vector<entt::entity> {dancer});
	EXPECT_EQ(dance.groups.all[1].parent, std::optional<std::size_t> {0});
	EXPECT_EQ(dance.groups.shared, (std::vector<std::size_t> {1, 0}));
	const auto& place = registry.Get<ecs::components::Dancer>(dancer);
	EXPECT_EQ(place.dance, entity);
	EXPECT_EQ(place.group, 0u);
	// a second is made after it; one whose dancer is not made is not made
	EXPECT_EQ(registry.Get<ecs::components::Dance>(saved_world_load::MakeDanceEntity(planned, objectOf)).made, dance.made + 1);
	const auto none = [](gamesave::ObjectNumber) { return entt::entity {entt::null}; };
	EXPECT_EQ(saved_world_load::MakeDanceEntity(planned, none), entt::entity {entt::null});
}

// A loaded town keeps its saved id as its key, or takes the next free key and keeps the id for the scripts; its player,
// tribe, neutral belief, area and record come from the save, and it makes no desire flags
TEST(SavedWorldLoad, ATownIsMadeWithItsSavedIdPlayerAndTribe)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	saved_world_load::PlannedTown planned {.number = 4};
	planned.record.id = 1;
	planned.record.playerNumber = 7;
	planned.record.tribe = static_cast<uint32_t>(Tribe::NORSE);
	planned.record.beliefInNeutralPlayer = 0.25f;
	planned.record.uninhabitable = 1;
	planned.record.areaMin = {.x = 100, .z = 200, .altitude = 0.0f};
	planned.record.areaMax = {.x = 300, .z = 400, .altitude = 0.0f};
	const auto first = saved_world_load::MakeTownEntity(planned);
	const auto second = saved_world_load::MakeTownEntity(planned);
	ASSERT_TRUE(registry.Valid(first));
	const auto& town = registry.Get<ecs::components::Town>(first);
	EXPECT_EQ(town.id, 1u);
	EXPECT_FALSE(town.scriptId.has_value());
	EXPECT_EQ(town.owner, PlayerNames::NEUTRAL);
	EXPECT_EQ(registry.Get<Tribe>(first), Tribe::NORSE);
	EXPECT_EQ(town.belief.beliefInNeutralPlayer, 0.25f);
	EXPECT_TRUE(town.uninhabitable);
	EXPECT_EQ(town.areaMin, glm::ivec2(100, 200));
	EXPECT_EQ(town.areaMax, glm::ivec2(300, 400));
	EXPECT_TRUE(registry.AllOf<ecs::components::LoadedRecord<gamesave::TownRecord>>(first));
	const auto& again = registry.Get<ecs::components::Town>(second);
	EXPECT_EQ(again.id, 2u);
	EXPECT_EQ(again.scriptId, 1u);
	// no desire flags: the two towns are the only things with a place
	EXPECT_EQ(registry.Size<ecs::components::Transform>(), 2u);
}

TEST(SavedWorldLoad, ThePotAndAbodeRowsRunDownFromTheirFirstRows)
{
	EXPECT_EQ(saved_world_load::PotTypeOfInfo(1874), PotInfo::StoragePitFoodPile);
	EXPECT_EQ(saved_world_load::PotTypeOfInfo(1873), PotInfo::WoodPile_1);
	EXPECT_EQ(saved_world_load::PotTypeOfInfo(1869), PotInfo::WoodPile_5);
	EXPECT_EQ(saved_world_load::AbodeTypeOfInfo(2275), AbodeInfo::NorseStoragePit);
	EXPECT_EQ(saved_world_load::AbodeTypeOfInfo(2355), AbodeInfo::AztecStoragePit);
	EXPECT_FALSE(saved_world_load::PotTypeOfInfo(1877).has_value());
	EXPECT_FALSE(saved_world_load::AbodeTypeOfInfo(2395).has_value());
}

// A storage pit takes the piles its record names, made before it, and the town of its record; the town's record
// names its storage pit
TEST(SavedWorldLoad, AStoragePitTakesItsSavedPilesAndTown)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::forestSystem> restoreForests;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	saved_world_load::WorldPlan plan;
	saved_world_load::PlannedTown town {.number = 5};
	town.record.id = 3;
	town.record.mainStoragePit.number = 12;
	plan.towns.push_back(town);
	plan.objects.emplace_back(saved_world_load::PlannedPile {
	    .number = 10, .type = PotInfo::StoragePitFoodPile, .record = gamesave::FoodPileRecord {}});
	plan.objects.emplace_back(
	    saved_world_load::PlannedPile {.number = 11, .type = PotInfo::WoodPile_1, .record = gamesave::WoodPileRecord {}});
	saved_world_load::PlannedStoragePit pit {.number = 12, .type = AbodeInfo::NorseStoragePit};
	pit.record.abode.town.number = 5;
	pit.record.foodPile.numbers = {10};
	pit.record.otherPiles.numbers = {11, 0, 0, 0, 0};
	plan.objects.emplace_back(pit);
	plan.census = {{gamesave::StoragePitRecord::k_Type, 1},
	               {gamesave::TownRecord::k_Type, 1},
	               {gamesave::FoodPileRecord::k_Type, 1},
	               {gamesave::WoodPileRecord::k_Type, 1}};

	std::map<gamesave::ObjectNumber, entt::entity> piles;
	uint32_t pitTown = 0;
	StoragePitPiles pitPiles;
	entt::entity madePit = entt::null;
	const saved_world_load::Makers makers {
	    .town =
	        [&registry](const saved_world_load::PlannedTown& planned) {
		        const auto entity = registry.Create();
		        registry.Assign<ecs::components::Town>(entity, ecs::components::Town {.id = planned.record.id});
		        return entity;
	        },
	    .pile =
	        [&registry, &piles](const saved_world_load::PlannedPile& pile) {
		        const auto entity = registry.Create();
		        piles[pile.number] = entity;
		        return entity;
	        },
	    .storagePit =
	        [&](const saved_world_load::PlannedStoragePit& /*planned*/, uint32_t townKey, const StoragePitPiles& given) {
		        pitTown = townKey;
		        pitPiles = given;
		        madePit = registry.Create();
		        return madePit;
	        },
	};
	const auto census = saved_world_load::Apply(plan, makers);
	EXPECT_EQ(pitTown, 3u);
	EXPECT_EQ(pitPiles.food, piles.at(10));
	EXPECT_EQ(pitPiles.wood.at(0), piles.at(11));
	EXPECT_EQ(pitPiles.wood.at(1), entt::entity {entt::null});
	// the town's storage pit
	entt::entity townsPit = entt::null;
	registry.Each<const ecs::components::Town>(
	    [&townsPit](entt::entity, const ecs::components::Town& made) { townsPit = made.storagePit; });
	EXPECT_EQ(townsPit, madePit);
	for (const auto& line : census)
	{
		EXPECT_EQ(line.made, line.saved) << line.type;
	}
}

// The footpaths come from the last of the game's footpath list to its first, each with its nodes in its own order; the
// links list the footpaths made, and the census counts both
TEST(SavedWorldLoad, TheFootpathsAndTheirLinksAreMadeInOrder)
{
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.lists[28].numbers = {2, 5};
	world.ResizeObjects(7);
	Put(world.objects[0], game);
	gamesave::FootpathRecord first;
	first.nodes.numbers = {3, 4};
	Put(world.objects[1], first);
	Put(world.objects[2], gamesave::FootpathNodeRecord {.coords = {.x = 1, .z = 2, .altitude = 0.0f}, .flags = 1});
	Put(world.objects[3], gamesave::FootpathNodeRecord {.coords = {.x = 3, .z = 4, .altitude = 0.0f}, .flags = 2});
	gamesave::FootpathRecord second;
	second.active = 1;
	Put(world.objects[4], second);
	gamesave::FootpathLinkRecord link;
	link.footpaths.numbers = {5, 2};
	Put(world.objects[5], link);
	Put(world.objects[6], gamesave::FootpathLinkRecord {});
	world.loadOrder = {3, 4, 2, 5, 6, 7, 1};

	const auto plan = saved_world_load::PlanOf(world);
	ASSERT_EQ(plan.footpaths.size(), 2u);
	EXPECT_EQ(plan.footpaths[0].number, 5u);
	EXPECT_EQ(plan.footpaths[1].number, 2u);
	ASSERT_EQ(plan.footpaths[1].nodes.size(), 2u);
	EXPECT_EQ(plan.footpaths[1].nodes[0].flags, 1);
	EXPECT_EQ(plan.footpaths[1].nodes[1].coords.x, 3);
	ASSERT_EQ(plan.footpathLinks.size(), 2u);

	std::vector<gamesave::ObjectNumber> footpaths;
	std::vector<std::vector<entt::entity>> links;
	const saved_world_load::Makers makers {
	    .footpath =
	        [&footpaths](const saved_world_load::PlannedFootpath& footpath) {
		        footpaths.push_back(footpath.number);
		        return static_cast<entt::entity>(footpath.number);
	        },
	    .footpathLink =
	        [&links](const saved_world_load::PlannedFootpathLink& made, const std::vector<entt::entity>& listed) {
		        links.push_back(listed);
		        return static_cast<entt::entity>(made.number);
	        },
	};
	const auto census = saved_world_load::Apply(plan, makers);
	const std::vector<gamesave::ObjectNumber> expectedFootpaths {5, 2};
	EXPECT_EQ(footpaths, expectedFootpaths);
	ASSERT_EQ(links.size(), 2u);
	const std::vector<entt::entity> expectedLink {static_cast<entt::entity>(5), static_cast<entt::entity>(2)};
	EXPECT_EQ(links[0], expectedLink);
	EXPECT_TRUE(links[1].empty());
	for (const auto type : {gamesave::FootpathRecord::k_Type, gamesave::FootpathLinkRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, line->saved);
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesFootpathsAreMadeWithTheirNodes)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	// its 84 footpaths with their 792 nodes, and its 59 links
	ASSERT_EQ(plan.footpaths.size(), 84u);
	size_t nodes = 0;
	for (const auto& footpath : plan.footpaths)
	{
		nodes += footpath.nodes.size();
	}
	EXPECT_EQ(nodes, 792u);
	EXPECT_EQ(plan.footpathLinks.size(), 59u);
}

// A citadel's heart has its citadel's player; a citadel's first heart keeps the citadel, and its other hearts are made
// of it
TEST(SavedWorldLoad, ACitadelsHeartsAreMadeOfTheCitadelItsFirstHeartKeeps)
{
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	gamesave::WorldRead world;
	world.ResizeObjects(5);
	Put(world.objects[0], gamesave::GameHead {});
	gamesave::PlayerRecord player;
	player.playerNumber = 1;
	Put(world.objects[1], player);
	gamesave::CitadelRecord citadel;
	citadel.container.owner.number = 2;
	Put(world.objects[2], citadel);
	gamesave::CitadelHeartRecord heart;
	heart.citadel.number = 3;
	Put(world.objects[3], heart);
	Put(world.objects[4], heart);
	world.loadOrder = {4, 5, 3, 2, 1};

	const auto plan = saved_world_load::PlanOf(world);
	ASSERT_EQ(plan.objects.size(), 2u);
	ASSERT_TRUE(std::holds_alternative<saved_world_load::PlannedCitadelHeart>(plan.objects[0]));
	EXPECT_EQ(std::get<saved_world_load::PlannedCitadelHeart>(plan.objects[0]).owner, PlayerNames::PLAYER_TWO);

	std::vector<entt::entity> citadels;
	const saved_world_load::Makers makers {
	    .citadelHeart =
	        [&citadels](const saved_world_load::PlannedCitadelHeart& made, entt::entity of) {
		        citadels.push_back(of);
		        return static_cast<entt::entity>(made.number);
	        },
	};
	const auto census = saved_world_load::Apply(plan, makers);
	const std::vector<entt::entity> expected {entt::null, static_cast<entt::entity>(4)};
	EXPECT_EQ(citadels, expected);
	for (const auto type : {gamesave::CitadelHeartRecord::k_Type, gamesave::CitadelRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, line->saved);
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldLoad, ALaterFrozenLandOneAutosavesTempleIsThePlayersAndBuilt)
{
	// the first frozen Land 1 autosave holds no citadel; this one, later in the land, does
	const auto bytes = FrozenSave(std::filesystem::path("_J_a_h_o_v_i_a") / "Saved Games" / "15" / "1" / "15.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen profile";
	}
	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	// the player's one heart, built
	std::vector<const saved_world_load::PlannedCitadelHeart*> hearts;
	for (const auto& object : plan.objects)
	{
		if (const auto* heart = std::get_if<saved_world_load::PlannedCitadelHeart>(&object); heart != nullptr)
		{
			hearts.push_back(heart);
		}
	}
	ASSERT_FALSE(hearts.empty());
	const auto players =
	    std::ranges::count_if(hearts, [](const auto* heart) { return heart->owner == PlayerNames::PLAYER_ONE; });
	EXPECT_EQ(players, 1);
	for (const auto* heart : hearts)
	{
		if (heart->owner == PlayerNames::PLAYER_ONE)
		{
			EXPECT_NE(heart->record.base.multi.fixedFlags & ecs::components::CitadelPartBuild::k_Built, 0);
		}
	}
}

// A loaded game puts back its creation counter: the next object made takes the saved number
TEST(SavedGameLoad, TheCreationCounterTakesTheSavedNumber)
{
	ecs::systems::ObjectCreationIndexSystem counter;
	counter.OnLoadMap();
	EXPECT_EQ(counter.Next(), 2u);
	counter.Restore(4321);
	EXPECT_EQ(counter.Peek(), 4321u);
	EXPECT_EQ(counter.Next(), 4321u);
	EXPECT_EQ(counter.Next(), 4322u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedGameLoad, TheFrozenLandOneAutosavesCounterIsPastEveryLoadedTreesNumber)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	const auto plan = saved_world_load::PlanOf(world);
	ASSERT_FALSE(plan.objects.empty());
	for (const auto& object : plan.objects)
	{
		if (const auto* tree = std::get_if<saved_world_load::PlannedTree>(&object); tree != nullptr)
		{
			ASSERT_LT(tree->record.base.object.creationIndex, state->createdObjects);
		}
	}
}
namespace
{
/// A load's makers with the trees' and big forests' filled in when a test leaves them out (they make nothing): the
/// load asks those two for every tree and big forest it plans
saved_world_load::Makers WithNoTreesMade(saved_world_load::Makers makers)
{
	if (!makers.tree)
	{
		makers.tree = [](const saved_world_load::PlannedTree&, uint32_t) { return entt::entity {entt::null}; };
	}
	if (!makers.bigForest)
	{
		makers.bigForest = [](const saved_world_load::PlannedBigForest&, uint32_t) { return entt::entity {entt::null}; };
	}
	return makers;
}
} // namespace

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesAbodesCrechesAndGraveyardsAreMadeInTheirTowns)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::forestSystem> restoreForests;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	std::map<uint32_t, uint32_t> byClass;
	uint32_t inATown = 0;
	const saved_world_load::Makers makers {
	    .town =
	        [&registry](const saved_world_load::PlannedTown& town) {
		        const auto entity = registry.Create();
		        registry.Assign<ecs::components::Town>(entity, ecs::components::Town {.id = town.record.id});
		        return entity;
	        },
	    .abode =
	        [&](const saved_world_load::PlannedAbode& abode, uint32_t townKey) {
		        ++byClass[std::visit([](const auto& saved) { return std::decay_t<decltype(saved)>::k_Type; }, abode.record)];
		        inATown += townKey != ecs::components::Abode::k_NoTown ? 1 : 0;
		        return registry.Create();
	        },
	};
	const auto census = saved_world_load::Apply(plan, WithNoTreesMade(makers));
	// its 30 abodes, 2 creches and 2 graveyards, every one of a town
	const std::map<uint32_t, uint32_t> expected {
	    {gamesave::AbodeRecord::k_Type, 30}, {gamesave::CrecheRecord::k_Type, 2}, {gamesave::GraveyardRecord::k_Type, 2}};
	EXPECT_EQ(byClass, expected);
	EXPECT_EQ(inATown, 34u);
	for (const auto& [type, count] : expected)
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, count);
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesDesireFlagsAreMadeForTheirTowns)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	const test::RestoreService<Locator::forestSystem> restoreForests;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	std::map<entt::entity, std::set<uint32_t>> desiresByTown;
	const saved_world_load::Makers makers {
	    .town =
	        [&registry](const saved_world_load::PlannedTown& town) {
		        const auto entity = registry.Create();
		        registry.Assign<ecs::components::Town>(entity, ecs::components::Town {.id = town.record.id});
		        return entity;
	        },
	    .desireFlag =
	        [&](const saved_world_load::PlannedDesireFlag& flag, entt::entity town) {
		        // a town has one flag for each desire at most
		        EXPECT_TRUE(desiresByTown[town].insert(flag.record.desireType).second);
		        return registry.Create();
	        },
	};
	const auto census = saved_world_load::Apply(plan, WithNoTreesMade(makers));
	const auto line = std::ranges::find(census, gamesave::TownDesireFlagsRecord::k_Type, &saved_world_load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_GT(line->saved, 0u);
	// every flag is of a town
	EXPECT_EQ(line->made, line->saved);
	EXPECT_EQ(desiresByTown.size(), plan.towns.size());
}

namespace
{
/// The reference saves the original wrote on this machine (_gamedata/original_saves): OPENBLACK_TEST_ORIGINAL_SAVES,
/// else the folder original_saves beside the game data's folder; none without either
std::optional<std::filesystem::path> OriginalSaves()
{
	std::error_code error;
	if (const char* saves = std::getenv("OPENBLACK_TEST_ORIGINAL_SAVES"); saves != nullptr) // NOLINT(concurrency-mt-unsafe)
	{
		return std::filesystem::is_directory(saves, error) ? std::optional(std::filesystem::path(saves)) : std::nullopt;
	}
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (game == nullptr)
	{
		return std::nullopt;
	}
	const auto beside = std::filesystem::path(game).parent_path() / "original_saves";
	return std::filesystem::is_directory(beside, error) ? std::optional(beside) : std::nullopt;
}

/// A place of the map, to a tenth of a metre, as the land scripts write their places
using ScriptPlace = std::pair<int64_t, int64_t>;

ScriptPlace PlaceOf(float x, float z)
{
	return {std::llround(x * 10.0f), std::llround(z * 10.0f)};
}

ScriptPlace PlaceOf(const gamesave::MapCoords& coords)
{
	return PlaceOf(map_coords::ToMetres(coords.x), map_coords::ToMetres(coords.z));
}

/// What a land script makes where: its trees' places with their types, its towns' places by id, its storage pits'
/// and big forests' places
struct ScriptPlaces
{
	std::map<ScriptPlace, uint32_t> trees;
	std::map<uint32_t, ScriptPlace> towns;
	std::set<ScriptPlace> storagePits;
	std::set<ScriptPlace> bigForests;
};

ScriptPlaces ReadScriptPlaces(const std::filesystem::path& path)
{
	ScriptPlaces places;
	std::ifstream stream(path);
	// (\x28 is the command's opening parenthesis)
	const std::regex tree(R"re(^CREATE_NEW_TREE\x28\s*-?\d+\s*,\s*"([-\d.]+),([-\d.]+)"\s*,\s*(\d+))re");
	const std::regex town(R"re(^CREATE_TOWN\x28\s*(\d+)\s*,\s*"([-\d.]+),([-\d.]+)")re");
	const std::regex pit(R"re(^CREATE_ABODE\x28\s*-?\d+\s*,\s*"([-\d.]+),([-\d.]+)"\s*,\s*"[A-Z]+_ABODE_STORAGE_PIT")re");
	const std::regex bigForest(R"re(^CREATE_NEW_BIG_FOREST\x28\s*"([-\d.]+),([-\d.]+)")re");
	std::string line;
	std::smatch match;
	while (std::getline(stream, line))
	{
		if (std::regex_search(line, match, tree))
		{
			places.trees[PlaceOf(std::stof(match[1]), std::stof(match[2]))] = static_cast<uint32_t>(std::stoul(match[3]));
		}
		else if (std::regex_search(line, match, town))
		{
			places.towns[static_cast<uint32_t>(std::stoul(match[1]))] = PlaceOf(std::stof(match[2]), std::stof(match[3]));
		}
		else if (std::regex_search(line, match, pit))
		{
			places.storagePits.insert(PlaceOf(std::stof(match[1]), std::stof(match[2])));
		}
		else if (std::regex_search(line, match, bigForest))
		{
			places.bigForests.insert(PlaceOf(std::stof(match[1]), std::stof(match[2])));
		}
	}
	return places;
}

/// A reference save of the original and what its file holds of the classes openblack loads
struct ReferenceSave
{
	std::filesystem::path file;
	std::string landscape;
	uint32_t land {0};
	std::string script;
	std::map<uint32_t, uint32_t> census;
	/// Saved early in its land, when most trees still stand at their script's places
	bool earlyInTheLand {true};
	/// Of each class, how many the load leaves out: held or in flight when saved, not standing on the map
	std::map<uint32_t, uint32_t> leftOut;
};
} // namespace

// Integration test: needs the original's reference saves (_gamedata/original_saves, OPENBLACK_TEST_ORIGINAL_SAVES or
// beside OPENBLACK_TEST_GAME_PATH) and the game's data; skipped without them. Each save the original wrote at a known
// moment is read and loaded as a load of it makes the world (with fake makers), and what is made is checked against
// the file and against the land's own script: every record read with every sum equal; the land, the turn and the
// clock; of every class openblack loads, as many made as the file holds; the towns, storage pits and big forests at
// their script's places, and the trees at a script place of the script's type
TEST(SavedWorldLoad, TheOriginalsReferenceSavesLoadAsTheirFilesSay)
{
	const auto saves = OriginalSaves();
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!saves.has_value() || game == nullptr)
	{
		GTEST_SKIP() << "no reference saves of the original (OPENBLACK_TEST_ORIGINAL_SAVES) or no game data";
	}
	const auto profile = std::filesystem::path("profile__e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                     "Saved Games" / "20" / "20.sav";
	using gamesave::AbodeRecord, gamesave::BigForestRecord, gamesave::CitadelHeartRecord, gamesave::CitadelRecord,
	    gamesave::CrecheRecord, gamesave::FoodPileRecord, gamesave::FootpathLinkRecord, gamesave::FootpathRecord,
	    gamesave::ForestRecord, gamesave::GraveyardRecord, gamesave::StoragePitRecord, gamesave::TownDesireFlagsRecord,
	    gamesave::TownRecord, gamesave::TreeRecord, gamesave::WoodPileRecord;
	const std::vector<ReferenceSave> references {// a new game, Land 1, the game's own autosave as the temple heart is built
	                                             {.file = *saves / "land1_new_answer3" / profile,
	                                              .landscape = ".\\DATA\\LANDSCAPE\\Land1.lnd",
	                                              .land = 1,
	                                              .script = "Land1.txt",
	                                              .census = {{TreeRecord::k_Type, 1376},
	                                                         {ForestRecord::k_Type, 9},
	                                                         {BigForestRecord::k_Type, 3},
	                                                         {TownRecord::k_Type, 6},
	                                                         {StoragePitRecord::k_Type, 2},
	                                                         {WoodPileRecord::k_Type, 10},
	                                                         {FoodPileRecord::k_Type, 2},
	                                                         {FootpathRecord::k_Type, 84},
	                                                         {FootpathLinkRecord::k_Type, 59},
	                                                         {AbodeRecord::k_Type, 30},
	                                                         {CrecheRecord::k_Type, 2},
	                                                         {GraveyardRecord::k_Type, 2},
	                                                         {CitadelHeartRecord::k_Type, 1},
	                                                         {CitadelRecord::k_Type, 1},
	                                                         {TownDesireFlagsRecord::k_Type, 42}}},
	                                             // the frozen Land 1 autosave resumed, saved again 120 s after the launch
	                                             {.file = *saves / "land1_autosave_resumed_120s" / profile,
	                                              .landscape = ".\\DATA\\LANDSCAPE\\Land1.lnd",
	                                              .land = 1,
	                                              .script = "Land1.txt",
	                                              .census = {{TreeRecord::k_Type, 1346},
	                                                         {ForestRecord::k_Type, 9},
	                                                         {BigForestRecord::k_Type, 3},
	                                                         {TownRecord::k_Type, 6},
	                                                         {StoragePitRecord::k_Type, 2},
	                                                         {WoodPileRecord::k_Type, 10},
	                                                         {FoodPileRecord::k_Type, 2},
	                                                         {FootpathRecord::k_Type, 84},
	                                                         {FootpathLinkRecord::k_Type, 59},
	                                                         {AbodeRecord::k_Type, 30},
	                                                         {CrecheRecord::k_Type, 2},
	                                                         {GraveyardRecord::k_Type, 2},
	                                                         {TownDesireFlagsRecord::k_Type, 42}}},
	                                             // the Land 2 autosave of a played game resumed, saved again 60 s after
	                                             // the launch
	                                             {.file = *saves / "land2_resumed_save60" / "slot20" / "20.sav",
	                                              .landscape = ".\\DATA\\LANDSCAPE\\Land2.lnd",
	                                              .land = 2,
	                                              .script = "Land2.txt",
	                                              .census = {{TreeRecord::k_Type, 746},
	                                                         {ForestRecord::k_Type, 26},
	                                                         {BigForestRecord::k_Type, 10},
	                                                         {TownRecord::k_Type, 15},
	                                                         {StoragePitRecord::k_Type, 11},
	                                                         {WoodPileRecord::k_Type, 55},
	                                                         {FoodPileRecord::k_Type, 15},
	                                                         {FootpathRecord::k_Type, 412},
	                                                         {FootpathLinkRecord::k_Type, 203},
	                                                         {AbodeRecord::k_Type, 111},
	                                                         {CrecheRecord::k_Type, 6},
	                                                         {GraveyardRecord::k_Type, 3},
	                                                         {CitadelHeartRecord::k_Type, 3},
	                                                         {CitadelRecord::k_Type, 3},
	                                                         {TownDesireFlagsRecord::k_Type, 105}},
	                                              .earlyInTheLand = false,
	                                              // one tree held when it was saved (its place unavailable)
	                                              .leftOut = {{TreeRecord::k_Type, 1}}}};
	for (const auto& reference : references)
	{
		SCOPED_TRACE(reference.file.generic_string());
		std::error_code error;
		ASSERT_TRUE(std::filesystem::is_regular_file(reference.file, error));
		const auto world = gamesave::ReadWorld(ReadAll(reference.file));
		EXPECT_EQ(world.stop, std::string(gamesave::k_ReadToTheEnd));
		EXPECT_FALSE(world.firstWrongSum.has_value());

		// the land, the turn and the clock
		const auto state = StateOf(world);
		ASSERT_TRUE(state.has_value());
		EXPECT_EQ(state->landscape, reference.landscape);
		EXPECT_EQ(state->landNumber, reference.land);
		EXPECT_EQ(state->lastAutoSaveTurn, state->gameTurn);
		EXPECT_EQ(state->clockMs, state->gameTurn * game_clock::k_MsPerTurn);
		EXPECT_LE(state->realGameTurn, state->gameTurn);

		// everything of the classes openblack loads is made, and where
		const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
		const test::RestoreService<Locator::forestSystem> restoreForests;
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
		std::vector<std::pair<uint32_t, ScriptPlace>> towns;
		std::vector<ScriptPlace> pits;
		std::vector<ScriptPlace> bigForests;
		std::vector<std::pair<ScriptPlace, TreeInfo>> trees;
		size_t nodes = 0;
		const auto any = [&registry]() { return registry.Create(); };
		const saved_world_load::Makers makers {
		    .footpath =
		        [&](const saved_world_load::PlannedFootpath& footpath) {
			        nodes += footpath.nodes.size();
			        return any();
		        },
		    .footpathLink = [&](const saved_world_load::PlannedFootpathLink&,
		                        const std::vector<entt::entity>&) { return any(); },
		    .town =
		        [&](const saved_world_load::PlannedTown& town) {
			        towns.emplace_back(town.record.id, PlaceOf(town.record.container.position.position));
			        const auto entity = any();
			        registry.Assign<ecs::components::Town>(entity, ecs::components::Town {.id = town.record.id});
			        return entity;
		        },
		    .tree =
		        [&](const saved_world_load::PlannedTree& tree, uint32_t) {
			        trees.emplace_back(PlaceOf(tree.record.base.position.position), tree.type);
			        return any();
		        },
		    .bigForest =
		        [&](const saved_world_load::PlannedBigForest& bigForest, uint32_t) {
			        bigForests.push_back(PlaceOf(bigForest.record.base.position.position));
			        return any();
		        },
		    .pile = [&](const saved_world_load::PlannedPile&) { return any(); },
		    .storagePit =
		        [&](const saved_world_load::PlannedStoragePit& pit, uint32_t, const StoragePitPiles&) {
			        pits.push_back(PlaceOf(pit.record.base.position.position));
			        return any();
		        },
		    .citadelHeart = [&](const saved_world_load::PlannedCitadelHeart&, entt::entity) { return any(); },
		    .abode = [&](const saved_world_load::PlannedAbode&, uint32_t) { return any(); },
		    .desireFlag = [&](const saved_world_load::PlannedDesireFlag&, entt::entity) { return any(); },
		};
		const auto census = saved_world_load::Apply(saved_world_load::PlanOf(world), makers);
		for (const auto& [type, count] : reference.census)
		{
			const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
			ASSERT_NE(line, census.end()) << gamesave::TypeName(type);
			EXPECT_EQ(line->saved, count) << gamesave::TypeName(type);
			const auto leftOut = reference.leftOut.contains(type) ? reference.leftOut.at(type) : 0u;
			EXPECT_EQ(line->made, count - leftOut) << gamesave::TypeName(type);
		}
		const auto savedNodes = world.Census().contains(gamesave::FootpathNodeRecord::k_Type)
		                            ? world.Census().at(gamesave::FootpathNodeRecord::k_Type)
		                            : 0u;
		EXPECT_EQ(nodes, savedNodes);

		// the towns, the storage pits and the big forests stand where the land's script made them; the trees at a
		// script place have that place's type
		const auto script = ReadScriptPlaces(std::filesystem::path(game) / "Scripts" / reference.script);
		ASSERT_FALSE(script.trees.empty());
		for (const auto& [id, place] : towns)
		{
			ASSERT_TRUE(script.towns.contains(id)) << id;
			EXPECT_EQ(place, script.towns.at(id)) << id;
		}
		// the script's storage pits and big forests are all there, unless the play has taken some (a pit built in play
		// stands at no script place)
		const auto atScript = [](const std::vector<ScriptPlace>& made, const std::set<ScriptPlace>& places) {
			return static_cast<size_t>(
			    std::ranges::count_if(made, [&places](const ScriptPlace& place) { return places.contains(place); }));
		};
		EXPECT_EQ(atScript(pits, script.storagePits), std::min(pits.size(), script.storagePits.size()));
		EXPECT_EQ(atScript(bigForests, script.bigForests), std::min(bigForests.size(), script.bigForests.size()));
		size_t atScriptPlaces = 0;
		for (const auto& [place, type] : trees)
		{
			if (const auto found = script.trees.find(place); found != script.trees.end())
			{
				++atScriptPlaces;
				EXPECT_EQ(static_cast<uint32_t>(type), found->second);
			}
		}
		// early in a land most of its trees still stand where the script made them (the others were planted or moved
		// since)
		if (reference.earlyInTheLand)
		{
			EXPECT_GT(atScriptPlaces * 10, trees.size() * 8);
		}
		EXPECT_GT(atScriptPlaces, 0u);
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's rivers have their script's points in their script's order, and its banks of mist their script's places,
// sizes, colours and shrinks
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesRiversAndMistsAreTheirScripts)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	const char* game = std::getenv("OPENBLACK_TEST_GAME_PATH"); // NOLINT(concurrency-mt-unsafe)
	if (!bytes.has_value() || game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	// the script's rivers (id, points in order) and mists (place, colour, size, shrink)
	std::map<uint32_t, std::vector<ScriptPlace>> scriptStreams;
	std::vector<std::tuple<ScriptPlace, uint32_t, float, float>> scriptMists;
	{
		std::ifstream stream(std::filesystem::path(game) / "Scripts" / "Land1.txt");
		const std::regex point(R"re(^CREATE_STREAM_POINT\x28\s*(\d+)\s*,\s*"([-\d.]+),([-\d.]+)")re");
		const std::regex mist(
		    R"re(^CREATE_MIST\x28\s*"([-\d.]+),([-\d.]+)"\s*,\s*[-\d.]+\s*,\s*(-?\d+)\s*,\s*([-\d.]+)\s*,\s*([-\d.]+))re");
		std::string line;
		std::smatch match;
		while (std::getline(stream, line))
		{
			if (std::regex_search(line, match, point))
			{
				scriptStreams[static_cast<uint32_t>(std::stoul(match[1]))].push_back(
				    PlaceOf(std::stof(match[2]), std::stof(match[3])));
			}
			else if (std::regex_search(line, match, mist))
			{
				scriptMists.emplace_back(PlaceOf(std::stof(match[1]), std::stof(match[2])),
				                         static_cast<uint32_t>(std::stol(match[3])), std::stof(match[4]), std::stof(match[5]));
			}
		}
	}
	ASSERT_FALSE(scriptStreams.empty());
	ASSERT_FALSE(scriptMists.empty());

	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	std::map<uint32_t, std::vector<ScriptPlace>> streams;
	std::vector<std::tuple<ScriptPlace, uint32_t, float, float>> mists;
	const saved_world_load::Makers makers {
	    .stream =
	        [&streams](const saved_world_load::PlannedStream& stream) {
		        auto& points = streams[stream.record.words[0]];
		        for (const auto& node : stream.record.nodes.values)
		        {
			        std::array<float, 3> point {};
			        std::memcpy(point.data(), node.data(), sizeof(point));
			        points.push_back(PlaceOf(point[0], point[2]));
		        }
		        return static_cast<entt::entity>(stream.number);
	        },
	    .mist =
	        [&mists](const saved_world_load::PlannedMist& mist) {
		        mists.emplace_back(PlaceOf(mist.record.position.position), mist.record.words[1],
		                           std::bit_cast<float>(mist.record.words[0]), std::bit_cast<float>(mist.record.words[2]));
		        return static_cast<entt::entity>(mist.number);
	        },
	};
	const auto census = saved_world_load::Apply(plan, WithNoTreesMade(makers));
	// the saved points are the floats the original kept from the script's, so a point may sit a tenth of a metre off
	// the script's text once both are rounded to tenths
	ASSERT_EQ(streams.size(), scriptStreams.size());
	for (const auto& [id, points] : scriptStreams)
	{
		ASSERT_TRUE(streams.contains(id)) << "stream " << id;
		const auto& loaded = streams.at(id);
		ASSERT_EQ(loaded.size(), points.size()) << "stream " << id;
		for (size_t i = 0; i < points.size(); ++i)
		{
			EXPECT_LE(std::llabs(loaded[i].first - points[i].first), 1) << "stream " << id << " point " << i;
			EXPECT_LE(std::llabs(loaded[i].second - points[i].second), 1) << "stream " << id << " point " << i;
		}
	}
	ASSERT_EQ(mists.size(), scriptMists.size());
	for (const auto& [place, colour, size, shrink] : mists)
	{
		const auto found =
		    std::ranges::find_if(scriptMists, [&place](const auto& script) { return std::get<0>(script) == place; });
		ASSERT_NE(found, scriptMists.end());
		EXPECT_EQ(colour, std::get<1>(*found));
		EXPECT_NEAR(size, std::get<2>(*found), 1e-5f);
		EXPECT_EQ(shrink, std::get<3>(*found));
	}
	for (const auto type : {gamesave::StreamRecord::k_Type, gamesave::MistRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, line->saved);
	}
}

TEST(SavedWorldLoad, TheClimateRowsRunDownFromTheWorldsRow)
{
	EXPECT_EQ(saved_world_load::ClimateTypeOfInfo(327), 0u);
	EXPECT_EQ(saved_world_load::ClimateTypeOfInfo(325), 2u);
	EXPECT_EQ(saved_world_load::ClimateTypeOfInfo(321), 6u);
	EXPECT_FALSE(saved_world_load::ClimateTypeOfInfo(320).has_value());
	EXPECT_FALSE(saved_world_load::ClimateTypeOfInfo(328).has_value());
}

TEST(SavedWorldLoad, TheVillagerRowsRunDownFromTheFirstAndALoadedCounterIsDrawnWhenNoStateLoadsValues)
{
	EXPECT_EQ(saved_world_load::VillagerTypeOfInfo(2247), static_cast<VillagerInfo>(0));
	EXPECT_EQ(saved_world_load::VillagerTypeOfInfo(2198), VillagerInfo::NorseHousewifeFemale);
	EXPECT_FALSE(saved_world_load::VillagerTypeOfInfo(2248).has_value());
	// created, with no final state: drawn; a building state, top or final, loads values: not drawn
	EXPECT_TRUE(saved_world_load::VillagerCounterDrawnOnLoad(85, 0));
	EXPECT_FALSE(saved_world_load::VillagerCounterDrawnOnLoad(39, 0));
	EXPECT_FALSE(saved_world_load::VillagerCounterDrawnOnLoad(85, 39));
}

TEST(SavedWorldLoad, TheMobileStaticRowsRunDownFromTheFirstToTheMeteorsAboveThePots)
{
	EXPECT_EQ(saved_world_load::MobileStaticTypeOfInfo(1937), MobileStaticInfo::CeltFenceShort);
	EXPECT_EQ(saved_world_load::MobileStaticTypeOfInfo(1935), MobileStaticInfo::Rock);
	EXPECT_EQ(saved_world_load::MobileStaticTypeOfInfo(1877), MobileStaticInfo::Meteor);
	EXPECT_FALSE(saved_world_load::MobileStaticTypeOfInfo(1876).has_value());
	EXPECT_FALSE(saved_world_load::MobileStaticTypeOfInfo(1938).has_value());
	EXPECT_EQ(saved_world_load::MobileStaticInfoRowOf(MobileStaticInfo::Meteor), 1877u);
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave's climates, newest first, are Land1.txt's: each id with its type, place, radii, rain, temperature and wind
TEST(SavedWorldLoad, TheFrozenLandOneAutosavesClimatesAreItsScripts)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto plan = saved_world_load::PlanOf(gamesave::ReadWorld(*bytes));
	// the game's own maker, into the weather the test services keep
	const saved_world_load::Makers makers {.climates = saved_world_load::MakeClimates};
	const test::RestoreService<Locator::forestSystem> restoreForests;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	const auto census = saved_world_load::Apply(plan, WithNoTreesMade(makers));
	std::vector<weather::climate::Climate> made;
	weather::climate::ForEach([&made](const weather::climate::Climate& climate) { made.push_back(climate); });
	ASSERT_TRUE(weather::climate::HasWorld());
	EXPECT_EQ(weather::climate::World().id, 0);
	// Land1.txt makes ids 3, 2, 1 then 0, each at the head: the list holds 0, 1, 2, 3
	ASSERT_EQ(made.size(), 4u);
	const std::array<int32_t, 4> ids {0, 1, 2, 3};
	const std::array<int32_t, 4> types {0, 2, 1, 1};
	const std::array<float, 4> temperatures {10.8f, -35.2f, 37.800014f, 37.800014f};
	const std::array<float, 4> targets {12.0f, -32.0f, 36.0f, 36.0f};
	const std::array<float, 4> winds {24.0f, 40.0f, 21.0f, 21.0f};
	const std::array<float, 4> desires {0.004632f, 0.630036f, 0.080639f, 0.078194f};
	const std::array<int32_t, 4> days {1, 97, 97, 97};
	const std::array<float, 4> outer {5120.0f, 321.666656f, 239.166656f, 257.5f};
	const std::array<std::pair<float, float>, 4> places {
	    {{2560.0f, 2560.0f}, {2701.29f, 2567.86f}, {2157.24f, 2425.05f}, {1740.66f, 3145.09f}}};
	for (size_t i = 0; i < made.size(); ++i)
	{
		SCOPED_TRACE(i);
		EXPECT_EQ(made[i].id, ids.at(i));
		EXPECT_EQ(made[i].info, types.at(i));
		EXPECT_FLOAT_EQ(made[i].temperature, temperatures.at(i));
		EXPECT_FLOAT_EQ(made[i].targetTemperature, targets.at(i));
		EXPECT_FLOAT_EQ(made[i].windX, winds.at(i));
		EXPECT_NEAR(made[i].rain.desire, desires.at(i), 1e-6f);
		EXPECT_EQ(made[i].rain.dryDays, days.at(i));
		EXPECT_FLOAT_EQ(made[i].outerRadius, outer.at(i));
		EXPECT_NEAR(map_coords::ToMetres(made[i].x), places.at(i).first, 0.01f);
		EXPECT_NEAR(map_coords::ToMetres(made[i].z), places.at(i).second, 0.01f);
		// the world climate is the one of id 0; it may have ten storms, a local one int(radius x 0.001 + 1)
		EXPECT_EQ(made[i].world, i == 0);
		EXPECT_EQ(made[i].maxStorms, i == 0 ? 10 : 1);
		// as many natural storms as its record holds, each in the storms' list
		EXPECT_EQ(made[i].storms.size(), plan.climates.at(i).record.areas.elements.size());
		for (const auto id : made[i].storms)
		{
			EXPECT_NE(weather::storms::Find(id), nullptr);
		}
	}
	const auto line = std::ranges::find(census, gamesave::ClimateRecord::k_Type, &saved_world_load::CensusLine::type);
	ASSERT_NE(line, census.end());
	EXPECT_EQ(line->made, line->saved);
	// each climate written back is its record, its natural storms the other way round, as a load leaves its list
	ecs::saved_world_write::ClassWriters writers;
	ecs::saved_world_write::RegisterWeatherWriters(writers);
	const ecs::Registry noEntities;
	const ecs::saved_world_write::WorldSource source {.registry = noEntities};
	ecs::saved_world_write::WriteContext context(writers, source);
	for (const auto& planned : plan.climates)
	{
		SCOPED_TRACE(planned.record.words[2]);
		const auto written = (*writers.SourceWriterOf(gamesave::ClimateRecord::k_Type))(context, planned.record.words[2]);
		ASSERT_TRUE(written.has_value());
		auto expected = planned.record;
		std::ranges::reverse(expected.areas.elements);
		EXPECT_EQ(std::get<gamesave::ClimateRecord>(*written), expected);
	}
	weather::climate::Reset();
}
// A storm's 80 bytes are its descriptor in order: the place, the radii, the times, the strength, the clouds, the fall
// speed, the lightning, the snow cover's rate, the unread word and the weather it adds
TEST(SavedWorldLoad, AStormsBytesAreItsDescriptorInOrder)
{
	std::array<uint8_t, 80> bytes {};
	const auto put = [&bytes]<typename T>(size_t at, T value) { std::memcpy(bytes.data() + at, &value, sizeof(T)); };
	put(0x00, 1.0f);
	put(0x04, 2.0f);
	put(0x08, 3.0f);
	put(0x0C, 40.0f);
	put(0x10, 90.0f);
	put(0x14, 5.0f);
	put(0x18, 60.0f);
	put(0x1C, 0.75f);
	put(0x20, int32_t {6});
	put(0x24, 0.25f);
	put(0x28, 200.0f);
	put(0x2C, 2.5f);
	put(0x30, 1.0f);
	put(0x34, 2.0f);
	put(0x38, 3.0f);
	put(0x3C, 4.0f);
	put(0x40, 1.5f);
	put(0x44, uint32_t {0xABCD});
	const std::array<int8_t, 8> weather {-5, 90, 10, 80, 12, -3, 7, 1};
	std::memcpy(bytes.data() + 0x48, weather.data(), weather.size());
	const auto d = saved_world_load::StormDescriptorOf(bytes);
	EXPECT_EQ(d.position, glm::vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(d.innerRadius, 40.0f);
	EXPECT_EQ(d.outerRadius, 90.0f);
	EXPECT_EQ(d.fadeInTime, 5.0f);
	EXPECT_EQ(d.lifeTime, 60.0f);
	EXPECT_EQ(d.strength, 0.75f);
	EXPECT_EQ(d.numClouds, 6);
	EXPECT_EQ(d.blackness, 0.25f);
	EXPECT_EQ(d.elevation, 200.0f);
	EXPECT_EQ(d.fallSpeed, 2.5f);
	EXPECT_EQ(d.sheetMin, 1.0f);
	EXPECT_EQ(d.sheetMax, 2.0f);
	EXPECT_EQ(d.forkMin, 3.0f);
	EXPECT_EQ(d.forkMax, 4.0f);
	EXPECT_EQ(d.snowCoverRate, 1.5f);
	EXPECT_EQ(d.unknown0x44, 0xABCDu);
	EXPECT_EQ(d.weather.temperature, -5);
	EXPECT_EQ(d.weather.rain, 90);
	EXPECT_EQ(d.weather.snow, 10);
	EXPECT_EQ(d.weather.overcast, 80);
	EXPECT_EQ(d.weather.windX, 12);
	EXPECT_EQ(d.weather.windZ, -3);
	EXPECT_EQ(d.weather.snowCover, 7);
	EXPECT_EQ(d.weather.stamp, 1);
}

// A climate's natural storms come back as the original's load makes them: each from its saved descriptor, put at the
// head of the climate's list (so the list is the other way round), with its saved age, speed and target
TEST(SavedWorldLoad, AClimatesNaturalStormsComeBackWithTheirAgeSpeedAndTarget)
{
	saved_world_load::PlannedClimate planned;
	planned.record.byte = 1; // the world's
	planned.record.words[4] = 327;
	for (int i = 0; i < 2; ++i)
	{
		gamesave::ClimateArea area;
		std::array<uint8_t, 80> bytes {};
		const float inner = 10.0f * static_cast<float>(i + 1);
		std::memcpy(bytes.data() + 0x0C, &inner, sizeof(float));
		area.values = bytes;
		area.words = {std::bit_cast<uint32_t>(3.0f + static_cast<float>(i)), std::bit_cast<uint32_t>(0.5f)};
		area.second = {100.0f, 0.0f, 200.0f + static_cast<float>(i)};
		planned.record.areas.elements.push_back(area);
	}
	ASSERT_EQ(saved_world_load::MakeClimates({planned}), 1u);
	ASSERT_TRUE(weather::climate::HasWorld());
	const auto& storms = weather::climate::World().storms;
	ASSERT_EQ(storms.size(), 2u);
	// the second saved is at the head
	const auto* head = weather::storms::Find(storms.front());
	const auto* last = weather::storms::Find(storms.back());
	ASSERT_NE(head, nullptr);
	ASSERT_NE(last, nullptr);
	EXPECT_EQ(head->descriptor.innerRadius, 20.0f);
	EXPECT_EQ(head->age, 4.0f);
	EXPECT_EQ(head->target, glm::vec3(100.0f, 0.0f, 201.0f));
	EXPECT_EQ(last->descriptor.innerRadius, 10.0f);
	EXPECT_EQ(last->age, 3.0f);
	EXPECT_EQ(last->speed, 0.5f);
	EXPECT_EQ(last->target, glm::vec3(100.0f, 0.0f, 200.0f));
	weather::climate::Reset();
	weather::storms::Clear();
}

// A storm's descriptor written gives back the bytes it was read from
TEST(SavedWorldLoad, AStormsDescriptorWritesBackItsBytes)
{
	std::array<uint8_t, 80> bytes {};
	for (size_t i = 0; i < bytes.size(); ++i)
	{
		bytes.at(i) = static_cast<uint8_t>(i * 7 + 3);
	}
	EXPECT_EQ(saved_world_load::StormDescriptorBytes(saved_world_load::StormDescriptorOf(bytes)), bytes);
}

// A loaded climate is written back as its record, its counters as the climate system holds them, and its natural
// storms in its list's order: the load put each at the head, so they come back the other way round, as the original's
// save after a load writes them
TEST(SavedWorldWrite, AClimateIsWrittenWithItsStormsInItsListsOrder)
{
	saved_world_load::PlannedClimate planned;
	planned.number = 9;
	planned.record.thing.flags = 4;
	planned.record.byte = 1;
	planned.record.point = {std::bit_cast<float>(0x01000000), std::bit_cast<float>(0x01000000), 3.0f};
	planned.record.words = {std::bit_cast<uint32_t>(5120.0f), std::bit_cast<uint32_t>(5120.0f), 0, 4, 327, 12, 2, 1};
	planned.record.word = 10;
	planned.record.lastWords = {500,
	                            std::bit_cast<uint32_t>(0.5f),
	                            std::bit_cast<uint32_t>(1.0f),
	                            std::bit_cast<uint32_t>(5.0f),
	                            std::bit_cast<uint32_t>(60.0f),
	                            std::bit_cast<uint32_t>(5.0f),
	                            std::bit_cast<uint32_t>(60.0f)};
	for (int i = 0; i < 2; ++i)
	{
		gamesave::ClimateArea area;
		std::array<uint8_t, 80> bytes {};
		const std::array<float, 3> place {100.0f + static_cast<float>(i), 7.0f, 200.0f};
		std::memcpy(bytes.data(), place.data(), sizeof(place));
		area.values = saved_world_load::StormDescriptorBytes(saved_world_load::StormDescriptorOf(bytes));
		area.point = place;
		area.words = {std::bit_cast<uint32_t>(3.0f + static_cast<float>(i)), std::bit_cast<uint32_t>(0.5f)};
		area.second = {100.0f, 0.0f, 200.0f + static_cast<float>(i)};
		planned.record.areas.elements.push_back(area);
	}
	ASSERT_EQ(saved_world_load::MakeClimates({planned}), 1u);
	ecs::saved_world_write::ClassWriters writers;
	ecs::saved_world_write::RegisterWeatherWriters(writers);
	ecs::Registry registry;
	const ecs::saved_world_write::WorldSource source {.registry = registry};
	ecs::saved_world_write::WriteContext context(writers, source);
	const auto written = (*writers.SourceWriterOf(gamesave::ClimateRecord::k_Type))(context, 0);
	ASSERT_TRUE(written.has_value());
	auto expected = planned.record;
	std::ranges::reverse(expected.areas.elements);
	EXPECT_EQ(std::get<gamesave::ClimateRecord>(*written), expected);
	weather::climate::Reset();
	weather::storms::Clear();
}

// A thing of the weather comes back at its place with its own descriptor, its storm made from the storm's saved
// descriptor with its age, speed and target, and is written back as its record; one whose storm is over has none
TEST(SavedWorldWrite, AWeatherThingComesBackWithItsStormAndIsWrittenAsItsRecord)
{
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	// the place's flags ask the map cells whether the thing is in the map
	const test::RestoreService<Locator::mapCellsSystem> restoreCells;
	Locator::mapCellsSystem::emplace<ecs::systems::MapCellsSystem>();
	ecs::saved_world_write::ClassWriters writers;
	ecs::saved_world_write::RegisterWeatherWriters(writers);
	for (const bool withStorm : {true, false})
	{
		SCOPED_TRACE(withStorm);
		saved_world_load::PlannedWeatherThing planned {.number = 12};
		planned.record.position.position = {.x = 0x0A00000, .z = 0x0B00000, .altitude = 0.0f};
		std::array<uint8_t, 80> own {};
		own.at(0x0C) = 0x40; // a radius's high byte
		planned.record.descriptor = saved_world_load::StormDescriptorBytes(saved_world_load::StormDescriptorOf(own));
		planned.record.affectedByWind = 1;
		if (withStorm)
		{
			std::array<uint8_t, 80> bytes {};
			const std::array<float, 3> place {1600.0f, 30.0f, 1700.0f};
			std::memcpy(bytes.data(), place.data(), sizeof(place));
			planned.record.storm = {.present = 1,
			                        .storm = gamesave::WeatherStorm {.point = place,
			                                                         .descriptor = saved_world_load::StormDescriptorBytes(
			                                                             saved_world_load::StormDescriptorOf(bytes)),
			                                                         .age = std::bit_cast<uint32_t>(12.5f),
			                                                         .speed = std::bit_cast<uint32_t>(2.0f),
			                                                         .target = {1650.0f, 0.0f, 1750.0f}}};
		}
		const auto entity = saved_world_load::MakeWeatherThingEntity(planned);
		const auto& thing = registry.Get<const ecs::components::WeatherThing>(entity);
		EXPECT_TRUE(thing.affectedByWind);
		EXPECT_EQ(weather::storms::Find(thing.storm) != nullptr, withStorm);
		const ecs::saved_world_write::WorldSource source {.registry = registry};
		ecs::saved_world_write::WriteContext context(writers, source);
		const auto written = (*writers.WriterOf(gamesave::WeatherThingRecord::k_Type))(context, entity);
		ASSERT_TRUE(written.has_value());
		EXPECT_EQ(std::get<gamesave::WeatherThingRecord>(*written), planned.record);
		weather::storms::Clear();
		weather::weather_thing::Reset();
	}
}

// The snow lying comes from the game object's tail: the grid's depths, the blocks' changes, the melting clock and band;
// the snow put back is the saved one
TEST(SavedGameLoad, TheSnowLyingComesBackAsSaved)
{
	gamesave::WorldRead world;
	gamesave::GameHead game;
	game.tail.snow.values.assign(snow_cover::k_Cells, 0);
	game.tail.snow.values.at(5) = std::bit_cast<uint32_t>(12.5f);
	game.tail.snowSecond.values.assign(1024, 0);
	game.tail.snowSecond.values.at(3) = std::bit_cast<uint32_t>(2.0f);
	game.tail.snowWords = {std::bit_cast<uint32_t>(0.25f), 3};
	Append(world, game);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	ASSERT_EQ(state->snowDepths.size(), snow_cover::k_Cells);
	EXPECT_EQ(state->snowDepths.at(5), 12.5f);
	EXPECT_EQ(state->snowBlockChanges.at(3), 2.0f);
	EXPECT_EQ(state->snowMeltClock, 0.25f);
	EXPECT_EQ(state->snowMeltBand, 3);

	ecs::systems::SnowSystem snow;
	EXPECT_FALSE(snow.HasSnow());
	snow.Restore(state->snowDepths, state->snowBlockChanges, state->snowMeltClock, state->snowMeltBand);
	EXPECT_TRUE(snow.HasSnow());
	EXPECT_EQ(snow.GetDepths()[5], 12.5f);
	// a grid of another size is left out
	snow.Restore(std::vector<float>(3, 1.0f), {}, 0.0f, 0);
	EXPECT_EQ(snow.GetDepths()[5], 12.5f);
}

// Integration test: needs the original's reference saves and the game's data; skipped without them. Every reference
// save holds the whole snow grid and the blocks' changes
TEST(SavedGameLoad, TheOriginalsReferenceSavesHoldTheWholeSnowGrid)
{
	const auto saves = OriginalSaves();
	if (!saves.has_value())
	{
		GTEST_SKIP() << "no reference saves of the original (OPENBLACK_TEST_ORIGINAL_SAVES)";
	}
	for (const auto& file :
	     {*saves / "land2_resumed_save60" / "slot20" / "20.sav",
	      *saves / "land1_new_answer3" / "profile__e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v" / "Saved Games" /
	          "20" / "20.sav"})
	{
		SCOPED_TRACE(file.generic_string());
		const auto state = StateOf(gamesave::ReadWorld(ReadAll(file)));
		ASSERT_TRUE(state.has_value());
		EXPECT_EQ(state->snowDepths.size(), snow_cover::k_Cells);
		EXPECT_EQ(state->snowBlockChanges.size(), 1024u);
		EXPECT_GE(state->snowMeltBand, 0);
		EXPECT_LT(state->snowMeltBand, 8);
		EXPECT_TRUE(std::ranges::all_of(state->snowDepths, [](float depth) { return depth >= 0.0f && depth <= 255.0f; }));
	}
}

// Integration test: needs the original game's data (OPENBLACK_TEST_GAME_PATH); skipped without it. The frozen Land 1
// autosave loaded the way the game's load makes its objects (each entity with its record kept), then written back from
// the entities: the game object by its writer from the values the load puts back, every entity the load made from its
// kept record, every other object from the kept world; the file is the original's, byte for byte
TEST(SavedWorldWrite, TheFrozenLandOneAutosaveWritesBackByteForByte)
{
	const auto bytes = FrozenSave(std::filesystem::path("_e_i_i_f_c_b_n_c_t_b_t_h_f_v_b_c_h_u_i_u_c_b_u_c_i_l_u_e_v") /
	                              "Saved Games" / "20" / "20.sav");
	if (!bytes.has_value())
	{
		GTEST_SKIP() << "OPENBLACK_TEST_GAME_PATH has no frozen Land 1 autosave";
	}
	const auto world = gamesave::ReadWorld(*bytes);
	ASSERT_EQ(world.stop, gamesave::k_ReadToTheEnd);
	const auto state = ecs::saved_game_load::StateOf(world);
	ASSERT_TRUE(state.has_value());

	const test::RestoreService<Locator::forestSystem> restore;
	Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	const test::RestoreService<Locator::entitiesRegistry> restoreRegistry;
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	// each maker makes an entity that keeps its whole record, as the game's makers do
	const auto keeping = [&registry]<typename Record>(const Record& record) {
		const auto entity = registry.Create();
		registry.Assign<ecs::components::LoadedRecord<Record>>(entity, record);
		return entity;
	};
	const auto keepingOne = [&keeping](const auto& variant) {
		return std::visit([&keeping](const auto& record) { return keeping(record); }, variant);
	};
	const saved_world_load::Makers makers {
	    .footpath = [&](const saved_world_load::PlannedFootpath& footpath) { return keeping(footpath.record); },
	    .footpathLink = [&](const saved_world_load::PlannedFootpathLink& link,
	                        const std::vector<entt::entity>& /*footpaths*/) { return keeping(link.record); },
	    .town = [&](const saved_world_load::PlannedTown& town) { return keeping(town.record); },
	    .tree = [&](const saved_world_load::PlannedTree& tree, uint32_t /*forest*/) { return keeping(tree.record); },
	    .bigForest = [&](const saved_world_load::PlannedBigForest& big, uint32_t /*forest*/) { return keeping(big.record); },
	    .pile = [&](const saved_world_load::PlannedPile& pile) { return keepingOne(pile.record); },
	    .storagePit = [&](const saved_world_load::PlannedStoragePit& pit, uint32_t /*town*/,
	                      const StoragePitPiles& /*piles*/) { return keeping(pit.record); },
	    .citadelHeart =
	        [&](const saved_world_load::PlannedCitadelHeart& heart, entt::entity citadel) {
		        const auto entity = keeping(heart.record);
		        if (citadel == entt::null)
		        {
			        registry.Assign<ecs::components::LoadedRecord<gamesave::CitadelRecord>>(entity, heart.citadel);
		        }
		        return entity;
	        },
	    .abode = [&](const saved_world_load::PlannedAbode& abode, uint32_t /*town*/) { return keepingOne(abode.record); },
	    .desireFlag = [&](const saved_world_load::PlannedDesireFlag& flag,
	                      entt::entity /*town*/) { return keeping(flag.record); },
	    .stream = [&](const saved_world_load::PlannedStream& stream) { return keeping(stream.record); },
	    .mist = [&](const saved_world_load::PlannedMist& mist) { return keeping(mist.record); },
	    .climates = [](const std::vector<saved_world_load::PlannedClimate>& climates) { return climates.size(); },
	};
	ecs::saved_world_write::LoadedObjects loaded;
	std::ignore = saved_world_load::Apply(saved_world_load::PlanOf(world), makers, &loaded);
	EXPECT_FALSE(loaded.empty());

	ecs::saved_world_write::ClassWriters writers;
	ecs::saved_world_write::RegisterGameObjectWriter(writers, [&state] { return ecs::saved_game_save::ValuesOf(*state); });
	const auto written = ecs::saved_world_write::WriteWorld(
	    writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(written.Complete()) << written.failure;

	// the loads' counter is 0, as the game's save leaves it; every other byte is the original's
	ASSERT_EQ(written.bytes.size(), bytes->size());
	uint32_t attempts = 1;
	std::memcpy(&attempts, written.bytes.data(), sizeof(attempts));
	EXPECT_EQ(attempts, 0u);
	constexpr std::ptrdiff_t k_Counter = sizeof(uint32_t);
	const auto difference =
	    std::mismatch(written.bytes.begin() + k_Counter, written.bytes.end(), bytes->begin() + k_Counter, bytes->end());
	EXPECT_TRUE(difference.first == written.bytes.end())
	    << "first difference at byte " << std::distance(written.bytes.begin(), difference.first);

	// the game object by its writer, each entity the load made from the record kept on it, the rest from the kept world
	std::map<ecs::saved_world_write::RecordSource, size_t> sources;
	for (const auto& record : written.records)
	{
		++sources[record.source];
	}
	EXPECT_EQ(sources[ecs::saved_world_write::RecordSource::Writer], 1u);
	// the loaded objects that are entities (the forests and the climates are kept by their keys)
	EXPECT_EQ(sources[ecs::saved_world_write::RecordSource::EntityRecord],
	          static_cast<size_t>(
	              std::ranges::count_if(loaded, [](const auto& entry) { return entry.second.entity != entt::null; })));
	EXPECT_EQ(written.records.size(), world.loadOrder.size());
	ASSERT_FALSE(written.records.empty());
	EXPECT_EQ(written.records.front().type, gamesave::k_GameType);

	// With the players' writer too: each player listed by the load and written from the values its load put back and
	// the objects the load made, the file is still the original's
	std::map<gamesave::ObjectNumber, entt::entity> entityOf;
	for (const auto& [number, object] : loaded)
	{
		entityOf.emplace(number, object.entity);
	}
	std::map<PlayerNames, const gamesave::PlayerRecord*> players;
	for (const auto& [number, record] : ecs::saved_players::ListPlayers(world))
	{
		loaded.insert_or_assign(number, ecs::saved_world_write::LoadedObject {
		                                    .entity = entt::null, .type = gamesave::k_PlayerType, .id = record->playerNumber});
		players.emplace(static_cast<PlayerNames>(record->playerNumber), record);
	}
	EXPECT_EQ(players.size(), gamesave::k_PlayerCount);
	const auto entityAt = [&entityOf](gamesave::ObjectNumber number) {
		const auto found = entityOf.find(number);
		return found != entityOf.end() ? found->second : entt::entity {entt::null};
	};
	ecs::saved_world_write::RegisterPlayerWriters(
	    writers, {.values = [&players](PlayerNames player) -> std::optional<ecs::saved_players::PlayerValues> {
		              return ecs::saved_players::ValuesOf(*players.at(player));
	              },
	              .objects =
	                  [&players, &entityAt](const ecs::Registry& /*registry*/, PlayerNames player) {
		                  ecs::saved_players::PlayerObjects objects {.citadel = entityAt(players.at(player)->citadel)};
		                  for (const auto town : players.at(player)->towns)
		                  {
			                  objects.towns.push_back(entityAt(town));
		                  }
		                  return objects;
	                  },
	              .newRecord = {}});
	const auto withPlayers = ecs::saved_world_write::WriteWorld(
	    writers, {.registry = registry, .landscape = world.framing.landscape, .kept = &world, .loaded = loaded});
	ASSERT_TRUE(withPlayers.Complete()) << withPlayers.failure;
	EXPECT_TRUE(withPlayers.bytes == written.bytes);
	const auto byPlayersWriter = std::ranges::count_if(withPlayers.records, [](const auto& record) {
		return record.type == gamesave::k_PlayerType && record.source == ecs::saved_world_write::RecordSource::Writer;
	});
	EXPECT_EQ(byPlayersWriter, static_cast<std::ptrdiff_t>(gamesave::k_PlayerCount));
}
