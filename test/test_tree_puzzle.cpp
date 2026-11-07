/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The tree puzzles (ecs::tree_puzzle_game, through ecs/PuzzleGames.h as the scripts reach it) on a flat dry land of the
// game's systems, zeroed tables, the trees' models boxes (each tree needs its model in the cache). See
// docs/wiki/world/tree-puzzles.md

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <algorithm>

#include <glm/trigonometric.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/TreePuzzle.h"
#include "ECS/HanoiRules.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/TreePuzzle.h"
#include "ECS/TreePuzzleRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/WorldHarness.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Land 2's tree puzzle: TreePuzzleOne's spot, turned 30 degrees
constexpr glm::vec3 k_Spot {2200.5f, 0.0f, 2400.25f};
constexpr float k_Angle = 30.0f;

class TreePuzzleWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// each kind of tree the grid plants: a box a metre across and four high, on synthetic data
		for (const auto kind : {TreeInfo::Conifer, TreeInfo::Oak, TreeInfo::Birch, TreeInfo::Beech})
		{
			LoadMesh(Info().tree.at(static_cast<size_t>(kind)).normal, glm::vec3(-0.5f, 0.0f, -0.5f),
			         glm::vec3(0.5f, 4.0f, 0.5f));
		}
		puzzle = CreatePuzzleGame(k_Spot, script::PuzzleGameType::Tree1, glm::radians(k_Angle), 1.0f);
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
	[[nodiscard]] TreePuzzle& Grid() const { return Reg().Get<TreePuzzle>(puzzle); }

	/// The place of a spot as the puzzle works it out: its grid from the game angle and the size
	static map_coords::MapCoords SpotPlace(int32_t across, int32_t along)
	{
		const auto steps = hanoi::StepsOf(static_cast<uint16_t>(hanoi::GameAngleOf(glm::radians(k_Angle))), 1.0f);
		const map_coords::MapCoords place {map_coords::ToFixed(k_Spot.x), map_coords::ToFixed(k_Spot.z), k_Spot.y};
		return hanoi::GridPlace(place, steps, across, along);
	}

	entt::entity puzzle {entt::null};
};
} // namespace

// Each of the nine spots has a small tree of the kind and full size its count shows, at its place on the grid, held by
// the scripts; the seven pulls at the start changed the counts; a lantern stands by each corner and a puff of mist over
// each spot
TEST_P(TreePuzzleWorld, TheGridIsPlantedWithItsTreesLanternsAndMist)
{
	ASSERT_TRUE(puzzle != entt::null);
	const auto& grid = Grid();
	int32_t changes = 0;
	for (int32_t across = 0; across < tree_puzzle::k_Side; ++across)
	{
		for (int32_t along = 0; along < tree_puzzle::k_Side; ++along)
		{
			const auto spot = tree_puzzle::SpotIndex(across, along);
			const auto tree = grid.trees.at(spot);
			ASSERT_TRUE(Reg().Valid(tree));
			const auto count = grid.counts.at(spot);
			changes += count;
			EXPECT_EQ(Reg().Get<const Tree>(tree).type, tree_puzzle::TreeKindOf(script::PuzzleGameType::Tree1, count));
			EXPECT_EQ(Reg().Get<const Tree>(tree).maxSize, tree_puzzle::FullSizeOf(script::PuzzleGameType::Tree1, count));
			const auto& at = Reg().Get<const Transform>(tree).position;
			EXPECT_NEAR(at.x, map_coords::ToMetres(SpotPlace(across, along).x), 0.01f);
			EXPECT_NEAR(at.z, map_coords::ToMetres(SpotPlace(across, along).z), 0.01f);
			EXPECT_TRUE(Reg().Valid(grid.mists.at(spot)));
		}
	}
	// seven pulls, each changing three to five spots
	EXPECT_GE(changes, 7 * 3);
	EXPECT_LE(changes, 7 * 5);
	for (const auto lantern : grid.lanterns)
	{
		EXPECT_TRUE(Reg().Valid(lantern));
	}
	EXPECT_FALSE(tree_puzzle_game::Started(puzzle));
}

// A tree on its spot grows by a twentieth a turn up to its full size
TEST_P(TreePuzzleWorld, ATreeOnItsSpotGrowsAStepATurn)
{
	auto& grid = Grid();
	const auto tree = grid.trees.at(4);
	const float before = Reg().Get<const Transform>(tree).scale.x;
	ProcessPuzzleGamesTurn();
	EXPECT_EQ(grid.trees.at(4), tree);
	const float full = Reg().Get<const Tree>(tree).maxSize;
	EXPECT_FLOAT_EQ(Reg().Get<const Transform>(tree).scale.x, std::min(before + tree_puzzle::k_GrowthStep, full));
}

// A tree taken off its spot is a pull of it: the spot and the four beside it change and are planted again, and the
// puzzle is in progress
TEST_P(TreePuzzleWorld, ATreeTakenOffItsSpotReplantsItAndTheFourBesideIt)
{
	auto& grid = Grid();
	const auto counts = grid.counts;
	const auto taken = grid.trees.at(tree_puzzle::SpotIndex(1, 1));
	Reg().Get<Transform>(taken).position.x += 3.0f;
	ProcessPuzzleGamesTurn();
	for (const auto& [across, along] : tree_puzzle::PulledSpots(1, 1))
	{
		const auto spot = tree_puzzle::SpotIndex(across, along);
		EXPECT_EQ(grid.counts.at(spot), static_cast<int8_t>(counts.at(spot) + 1));
	}
	// the corners stay as they were
	EXPECT_EQ(grid.counts.at(tree_puzzle::SpotIndex(0, 0)), counts.at(tree_puzzle::SpotIndex(0, 0)));
	EXPECT_NE(grid.trees.at(tree_puzzle::SpotIndex(1, 1)), taken);
	EXPECT_FALSE(ecs::IsAvailable(taken));
	EXPECT_TRUE(tree_puzzle_game::Started(puzzle));
}

// A tree lifted straight up is off its spot too: its place's altitude is part of the comparison
TEST_P(TreePuzzleWorld, ATreeLiftedStraightUpIsOffItsSpot)
{
	auto& grid = Grid();
	const auto spot = tree_puzzle::SpotIndex(2, 0);
	const auto lifted = grid.trees.at(spot);
	Reg().Get<Transform>(lifted).position.y += 2.0f;
	ProcessPuzzleGamesTurn();
	EXPECT_NE(grid.trees.at(spot), lifted);
	EXPECT_TRUE(tree_puzzle_game::Started(puzzle));
}

// A tree that went is put back the same way
TEST_P(TreePuzzleWorld, ATreeThatWentIsPutBack)
{
	auto& grid = Grid();
	const auto spot = tree_puzzle::SpotIndex(0, 2);
	const auto gone = grid.trees.at(spot);
	ToBeDeleted(gone, true);
	ProcessPuzzleGamesTurn();
	EXPECT_NE(grid.trees.at(spot), gone);
	EXPECT_TRUE(Reg().Valid(grid.trees.at(spot)));
}

// Solved when every tree is at its full size and every spot shows the same state: the puzzle lets go of the trees and
// the mist goes, so the test answers no again afterwards, as the original's
TEST_P(TreePuzzleWorld, AllGrownInOneStateIsTheSolutionOnceOnly)
{
	auto& grid = Grid();
	EXPECT_FALSE(IsPuzzleGamePlayed(puzzle));
	for (size_t spot = 0; spot < tree_puzzle::k_Spots; ++spot)
	{
		grid.counts.at(spot) = 2;
		auto& tree = Reg().Get<Tree>(grid.trees.at(spot));
		tree.maxSize = 1.0f;
		Reg().Get<Transform>(grid.trees.at(spot)).scale = glm::vec3(1.0f);
	}
	const auto mist = grid.mists.front();
	EXPECT_TRUE(IsPuzzleGamePlayed(puzzle));
	for (const auto tree : grid.trees)
	{
		EXPECT_EQ(tree, entt::entity {entt::null});
	}
	EXPECT_FALSE(ecs::IsAvailable(mist));
	EXPECT_FALSE(IsPuzzleGamePlayed(puzzle));
}

// The rules alone decide here, so synthetic data only
INSTANTIATE_TEST_SUITE_P(Data, TreePuzzleWorld, ::testing::Values(test::world::WorldData::Synthetic), test::world::DataName);
