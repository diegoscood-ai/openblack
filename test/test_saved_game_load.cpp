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

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <utility>
#include <variant>
#include <vector>

#include <LHVM.h>
#include <SavedRecords.h>
#include <WorldReader.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/SavedGameLoad.h"
#include "ECS/SavedWorldLoad.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Trees.h"
#include "Locator.h"
#include "support/RestoreService.h"

using namespace openblack;
using openblack::ecs::saved_game_load::StateOf;
namespace saved_world_load = openblack::ecs::saved_world_load;

namespace
{
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
	game.data = {11, 22, 1500, 900, 0, 0, 0, 0};
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
	world.objects.emplace_back(game);
	const auto state = StateOf(world);
	ASSERT_TRUE(state.has_value());
	EXPECT_EQ(state->landscape, ".\\DATA\\LANDSCAPE\\Land2.lnd");
	EXPECT_EQ(state->landNumber, 2u);
	EXPECT_EQ(state->seeds.synced, 11u);
	EXPECT_EQ(state->seeds.local, 22u);
	EXPECT_EQ(state->gameTurn, 1500u);
	EXPECT_EQ(state->realGameTurn, 900u);
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
	world.objects.resize(8);
	world.objects[0] = game;
	world.objects[1] = SavedForest(5, 100, 9, true);
	world.objects[2] = SavedForest(5, 100, 9, false);
	world.objects[3] = SavedTree(2115, 1, 2);
	world.objects[4] = SavedTree(2103, 2, 3);
	auto held = SavedTree(2110, 3, 0);
	held.base.position.flags = 1U << 2U;
	world.objects[5] = held;
	world.objects[6] = SavedTree(500, 4, 0);
	world.objects[7] = SavedBigForest(368, 3);
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

	const test::RestoreService<Locator::forestSystem> restore;
	auto& forests = Locator::forestSystem::emplace<ecs::systems::ForestSystem>();
	uint32_t inAForest = 0;
	uint32_t bigForestsInAForest = 0;
	const saved_world_load::Makers makers {
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
	};
	const auto census = saved_world_load::Apply(plan, makers);
	EXPECT_EQ(forests.All().size(), 9u);
	EXPECT_EQ(inAForest, 581u);
	// every big forest has its forest, and that forest points back at it
	EXPECT_EQ(bigForestsInAForest, 3u);
	EXPECT_EQ(std::ranges::count_if(forests.All(), [](const auto& entry) { return entry.second.bigForest != entt::null; }), 3);
	for (const auto type : {gamesave::TreeRecord::k_Type, gamesave::BigForestRecord::k_Type})
	{
		const auto line = std::ranges::find(census, type, &saved_world_load::CensusLine::type);
		ASSERT_NE(line, census.end());
		EXPECT_EQ(line->made, line->saved);
	}
}
