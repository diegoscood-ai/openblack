/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A town's scenic forest (ecs::MakeScenicForest): which trees about the town's centre it takes, with raffclar's cases
// (0222f9f5, test_scenic_forest.cpp). No game data: a fake info table, the map cells and the forest list.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <bit>
#include <memory>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
class ScenicForestTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->town.maxDistanceForTownForest = 250.0f;
		// every tree row of info.dat is a FOREST_TREE (the map cells read the row's type)
		for (auto& tree : info->tree)
		{
			tree.type = ObjectType::ForestTree;
		}
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::EmplaceWorldSystems();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
	}
	void TearDown() override
	{
		ecs::ClearForests();
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A tree in the map cells at (x, 1000) metres
	static entt::entity TreeAt(float x)
	{
		const auto tree = Reg().Create();
		ecs::object_index::Assign(tree);
		Reg().Assign<Transform>(tree, glm::vec3(x, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		Reg().Assign<Tree>(tree);
		ecs::map_cells::InsertMapObject(tree);
		return tree;
	}
	static uint32_t ForestOf(entt::entity tree) { return Reg().Get<const Tree>(tree).forestId; }

private:
	test::RestoreService<Locator::infoConstants> _info;
};
} // namespace

TEST_F(ScenicForestTest, ATreeInNoForestIsTakenAndOneInAnotherForestIsNot)
{
	const auto lone = TreeAt(1050.0f);
	const auto forested = TreeAt(1050.0f);
	const auto other = ecs::CreateForest(0, glm::vec3(1200.0f, 0.0f, 1000.0f));
	ecs::SetTreeForest(forested, other);
	ecs::MakeScenicForest(1, glm::vec3(1000.0f, 0.0f, 1000.0f));
	ASSERT_NE(ForestOf(lone), 0u);
	EXPECT_TRUE(ecs::IsScenicForest(ForestOf(lone)));
	EXPECT_EQ(ForestOf(forested), other);
}

TEST_F(ScenicForestTest, ATreeOfAnotherTownsScenicForestComesOnlyWhenStrictlyNearer)
{
	const auto nearer = TreeAt(1040.0f);
	const auto halfway = TreeAt(1050.0f);
	// the other town's scenic forest, about (1100, 1000), takes both first
	ecs::MakeScenicForest(2, glm::vec3(1100.0f, 0.0f, 1000.0f));
	const auto theirs = ForestOf(nearer);
	ASSERT_NE(theirs, 0u);
	ASSERT_EQ(ForestOf(halfway), theirs);
	// 40 m from this town's centre against 60 from theirs: taken; 50 against 50: left to them
	ecs::MakeScenicForest(1, glm::vec3(1000.0f, 0.0f, 1000.0f));
	EXPECT_NE(ForestOf(nearer), theirs);
	EXPECT_TRUE(ecs::IsScenicForest(ForestOf(nearer)));
	EXPECT_EQ(ForestOf(halfway), theirs);
}

// The spots round a parent tree turn by the original's float between rings, a sixteenth of a half turn
TEST(TreeRings, TheRingsAreASixteenthOfAHalfTurnApart)
{
	EXPECT_EQ(std::bit_cast<uint32_t>(ecs::k_TreeRingStep), 0x3E490FDBu);
}

// The spot next to a tree: five radii a ring, the first drawn and then (r + 2) mod 10, and the next ring a sixteenth of
// a half turn on with a radius of its own; the first free spot
TEST(TreeRings, TheSpotIsTheFirstFreeOfFiveRadiiARingAndThenTheNextRing)
{
	const auto origin = map_coords::FromMetres(glm::vec2(1000.0f, 2000.0f));
	const float angle = 0.25f;
	std::vector<uint32_t> radii {9, 5};
	size_t draws = 0;
	std::vector<map_coords::MapCoords> tried;
	const auto spot = ecs::FindTreeSpot(
	    origin, angle, [&] { return radii.at(draws++); },
	    [&tried](const map_coords::MapCoords& coords) {
		    tried.push_back(coords);
		    return tried.size() == 8;
	    });
	ASSERT_TRUE(spot.has_value());
	EXPECT_EQ(draws, 2u);
	const std::vector<std::pair<int, float>> expected {{0, 9.0f}, {0, 1.0f}, {0, 3.0f}, {0, 5.0f},
	                                                   {0, 7.0f}, {1, 5.0f}, {1, 7.0f}, {1, 9.0f}};
	ASSERT_EQ(tried.size(), expected.size());
	for (size_t i = 0; i < expected.size(); ++i)
	{
		const float ringAngle = expected[i].first == 0 ? angle : angle + ecs::k_TreeRingStep;
		const auto at = origin + gutils::GetPosFromAngle(ringAngle, expected[i].second);
		EXPECT_EQ(tried[i].x, at.x) << i;
		EXPECT_EQ(tried[i].z, at.z) << i;
	}
	EXPECT_EQ(spot->x, tried.back().x);
	EXPECT_EQ(spot->z, tried.back().z);
}

TEST(TreeRings, NoFreeSpotAfterThirtyTwoRingsOfFive)
{
	size_t draws = 0;
	size_t tries = 0;
	const auto spot = ecs::FindTreeSpot(
	    map_coords::FromMetres(glm::vec2(100.0f, 100.0f)), 0.0f,
	    [&draws] {
		    ++draws;
		    return 5u;
	    },
	    [&tries](const map_coords::MapCoords& /*unused*/) {
		    ++tries;
		    return false;
	    });
	EXPECT_FALSE(spot.has_value());
	EXPECT_EQ(draws, 32u);
	EXPECT_EQ(tries, 160u);
}

// A forest plants when attempts x min(1, 0.05 x grown) x turns since the last tree / 300 is above its threshold, and
// only with a grown tree; the parent is drawn from the nearest n / 2 + 1
TEST(TreeGrowth, AForestPlantsWhenItsAttemptsOutgrowItsThreshold)
{
	EXPECT_FALSE(ecs::ForestPlantsATree(1.0f, 20, 600000.0f, 2000.0f));
	EXPECT_TRUE(ecs::ForestPlantsATree(1.0f, 20, 600300.0f, 2000.0f));
	EXPECT_TRUE(ecs::ForestPlantsATree(1.0f, 40, 600300.0f, 2000.0f));
	EXPECT_FALSE(ecs::ForestPlantsATree(1.0f, 10, 600300.0f, 2000.0f));
	EXPECT_TRUE(ecs::ForestPlantsATree(2.0f, 10, 600300.0f, 2000.0f));
	EXPECT_FALSE(ecs::ForestPlantsATree(1e9f, 0, 1e9f, 2000.0f));
	// multiplied as the original does, the turns by a three-hundredth first: here a unit in the last place above what
	// dividing the whole by 300 gives, so it is just over this threshold
	EXPECT_TRUE(ecs::ForestPlantsATree(2671.0f, 30, 138947.0f, std::bit_cast<float>(0x4997031Bu)));

	EXPECT_EQ(ecs::ParentTreeDraws(1), 1u);
	EXPECT_EQ(ecs::ParentTreeDraws(2), 2u);
	EXPECT_EQ(ecs::ParentTreeDraws(3), 2u);
	EXPECT_EQ(ecs::ParentTreeDraws(20), 11u);
}

// A forest tree's growth counter ticks down before it is read: due the turn it reaches 0, then growTurns again; one
// drawn 0 wraps round first and waits 65536 turns
TEST(TreeGrowth, TheCounterTicksBeforeItIsRead)
{
	uint16_t counter = 1;
	EXPECT_TRUE(ecs::TickGrowCounter(counter, 10));
	EXPECT_EQ(counter, 10);
	for (int turn = 1; turn < 10; ++turn)
	{
		EXPECT_FALSE(ecs::TickGrowCounter(counter, 10)) << turn;
	}
	EXPECT_TRUE(ecs::TickGrowCounter(counter, 10));

	uint16_t drawnZero = 0;
	EXPECT_FALSE(ecs::TickGrowCounter(drawnZero, 10));
	EXPECT_EQ(drawnZero, 65535);
	for (int turn = 1; turn < 65535; ++turn)
	{
		ASSERT_FALSE(ecs::TickGrowCounter(drawnZero, 10)) << turn;
	}
	EXPECT_TRUE(ecs::TickGrowCounter(drawnZero, 10));
}

// A forest's id: the counter's for one made without an id (the counter then goes up by one), and a given id raises the
// counter past it only when the counter is below it, so two forests may share an id; each has its own key, and a
// script's tree joins the newest forest with its id
TEST_F(ScenicForestTest, ForestIdsFollowTheCounterAndMayRepeat)
{
	const glm::vec3 at {1000.0f, 0.0f, 1000.0f};
	const auto first = ecs::CreateForest(0, at);
	const auto again = ecs::CreateForest(1, at);
	const auto second = ecs::CreateForest(0, at);
	const auto fifth = ecs::CreateForest(5, at);
	const auto sixth = ecs::CreateForest(0, at);
	const auto fifthAgain = ecs::CreateForest(5, at);
	const std::vector<uint32_t> keys {first, again, second, fifth, sixth, fifthAgain};
	const std::vector<uint32_t> expectedKeys {1, 2, 3, 4, 5, 6};
	EXPECT_EQ(keys, expectedKeys);
	std::vector<uint32_t> ids;
	for (const auto key : keys)
	{
		ids.push_back(Locator::forestSystem::value().All().at(key).id);
	}
	const std::vector<uint32_t> expectedIds {1, 1, 2, 5, 6, 5};
	EXPECT_EQ(ids, expectedIds);
	EXPECT_EQ(ecs::ResolveForestId(1), again);
	EXPECT_EQ(ecs::ResolveForestId(2), second);
	EXPECT_EQ(ecs::ResolveForestId(5), fifthAgain);
	EXPECT_EQ(ecs::ResolveForestId(7), 0u);
	EXPECT_EQ(ecs::ResolveForestId(-1), 0u);
}

// A new tree grows only when its maximum differs from its size, and only then draws the turn of its first growth step
// from the synchronised stream; a tree that does not grow draws nothing
TEST_F(ScenicForestTest, ANewTreeDrawsItsFirstGrowthTurnOnlyWhenItGrows)
{
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->tree.at(static_cast<size_t>(TreeInfo::Oak)).growsAfterNumGameTurns = 300;
	Locator::infoConstants::reset(info.release());
	game_random::Load({.synced = 12345u, .local = 678u});

	const auto grown = ecs::archetypes::TreeArchetype::NewGrowth(TreeInfo::Oak, 1.0f, 1.0f);
	EXPECT_FALSE(grown.growing);
	EXPECT_EQ(grown.counter, 0);
	EXPECT_EQ(game_random::Current().synced, 12345u);

	uint32_t seed = 12345u;
	const auto expected = game_random::SeededRandom(300, seed);
	const auto growing = ecs::archetypes::TreeArchetype::NewGrowth(TreeInfo::Oak, 1.2f, 0.1f);
	EXPECT_TRUE(growing.growing);
	EXPECT_EQ(growing.counter, expected);
	EXPECT_EQ(game_random::Current().synced, seed);
}

TEST_F(ScenicForestTest, AnEmptyForestIsDeletedOnItsTwoThousandthEmptyTurn)
{
	// the tree's row is worth some wood, so that the town takes the forest in
	auto info = std::make_unique<InfoConstants>(Locator::infoConstants::value());
	info->tree.at(static_cast<size_t>(TreeInfo::Beech)).woodValue = 100;
	Locator::infoConstants::reset(info.release());
	const glm::vec3 centre {1000.0f, 0.0f, 1000.0f};
	const auto forest = ecs::CreateForest(0, centre);
	const auto tree = TreeAt(1000.0f);
	ecs::SetTreeForest(tree, forest);
	ecs::AssignForestsToTown(1, centre);
	ASSERT_EQ(ecs::TownForests(1), std::vector<uint32_t> {forest});
	// a forest with no trees but a big forest is not empty
	const auto kept = ecs::CreateForest(0, centre + glm::vec3(100.0f, 0.0f, 0.0f));
	ecs::SetForestBigForest(kept, Reg().Create());
	ecs::DeleteTree(tree);

	// the first empty turn sets the wait to 2000, each later one takes one off, and it goes when the wait falls below
	// 2: on its 2000th empty turn
	for (uint32_t turn = 1; turn < 2000; ++turn)
	{
		ecs::ProcessTreesTurn(turn);
	}
	EXPECT_TRUE(ecs::IsInForest(forest));
	EXPECT_EQ(ecs::TownForests(1), std::vector<uint32_t> {forest});
	ecs::ProcessTreesTurn(2000);
	EXPECT_FALSE(ecs::IsInForest(forest));
	EXPECT_TRUE(ecs::TownForests(1).empty());
	for (uint32_t turn = 2001; turn <= 4000; ++turn)
	{
		ecs::ProcessTreesTurn(turn);
	}
	EXPECT_TRUE(ecs::IsInForest(kept));
}
