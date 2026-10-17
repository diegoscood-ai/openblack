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

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

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
