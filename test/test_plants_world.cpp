/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The bushes, copses and hedges of the land are kinds of tree: each is made as a tree of its row of the trees' table,
// sways with the trees facing its way, and grows in its forest turn by turn as any tree does, by its row's growth,
// up to its full size. Played in a small world of the game's own systems, with the game's data when it is there and a
// synthetic table otherwise.
// docs/wiki/project/progress/nature/plants_and_flowers.md, docs/wiki/world/trees.md

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Systems/Implementations/DayNightClockSystem.h"
#include "ECS/Trees.h"
#include "support/WorldHarness.h"

using namespace openblack;
using ecs::components::Transform;
using ecs::components::Tree;

namespace
{
constexpr std::array k_Plants {TreeInfo::Bush,   TreeInfo::BushA, TreeInfo::BushB, TreeInfo::Copse,
                               TreeInfo::CopseA, TreeInfo::Hedge, TreeInfo::HedgeA};

class PlantsWorld: public test::world::WorldHarness
{
protected:
	void SetUp() override
	{
		WorldHarness::SetUp();
		if (IsSkipped() || HasFatalFailure())
		{
			return;
		}
		// the clock the weather asks a tree's rain of
		Locator::dayNightClock::emplace<ecs::systems::DayNightClockSystem>();
		for (const auto plant : k_Plants)
		{
			auto& row = Info().tree.at(static_cast<size_t>(plant));
			if (!GameData())
			{
				// a synthetic row that grows every turn by a tenth
				row.type = ObjectType::ForestTree;
				row.normal = MeshId::TreeBeech;
				row.growsAfterNumGameTurns = 1;
				row.growthAmount = 0.1f;
			}
			LoadMesh(row.normal, glm::vec3(-1.0f, 0.0f, -1.0f), glm::vec3(1.0f, 3.0f, 1.0f));
		}
	}

	void TearDown() override
	{
		// a skipped world has no forests to clear
		if (Locator::forestSystem::has_value())
		{
			ecs::ClearForests();
		}
		WorldHarness::TearDown();
	}

	void BeforePhysics() override { ecs::ProcessTreesTurn(_turn++); }

private:
	test::RestoreService<Locator::dayNightClock> _restoreClock;
	uint32_t _turn {0};
};
} // namespace

// Each is a tree of its own row, swaying with the trees that face its way, and grows in its forest as a tree does, turn
// by turn up to its full size
TEST_P(PlantsWorld, BushesCopsesAndHedgesAreTreesThatGrowAsTreesDo)
{
	// a forest of the land, which its trees grow in (a scenic forest's trees do not grow on their own)
	const auto forest = ecs::CreateForest(0, glm::vec3(2560.0f, k_Ground, 2500.0f));
	std::vector<entt::entity> plants;
	float x = 2500.0f;
	for (size_t i = 0; i < k_Plants.size(); ++i)
	{
		const auto plant = k_Plants.at(i);
		// facing the way of wind slot i, as a tree turned so takes it
		const float yaw = static_cast<float>(i) * glm::two_pi<float>() / 16.0f;
		// growing, its first step next turn (a new tree's first step falls on a random turn of its row's period)
		const auto entity = ecs::archetypes::TreeArchetype::Create(forest, glm::vec3(x, k_Ground, 2500.0f), plant, false, yaw,
		                                                           1.0f, 0.5f, {.growing = true, .counter = 1});
		x += 20.0f;
		ASSERT_TRUE(Registry().AllOf<Tree>(entity)) << static_cast<int>(plant);
		EXPECT_EQ(Registry().Get<const Tree>(entity).type, plant);
		// it sways with the trees facing its way
		EXPECT_EQ(Registry().Get<const Tree>(entity).windSlot, i) << static_cast<int>(plant);
		plants.push_back(entity);
	}
	uint32_t longest = 0;
	for (const auto plant : k_Plants)
	{
		longest = std::max(longest, Info().tree.at(static_cast<size_t>(plant)).growsAfterNumGameTurns);
	}
	// long enough for each to take a few growth steps
	for (uint32_t turn = 0; turn < 4 * longest + 4; ++turn)
	{
		Turn();
	}
	for (size_t i = 0; i < plants.size(); ++i)
	{
		const auto& row = Info().tree.at(static_cast<size_t>(k_Plants.at(i)));
		const float size = Registry().Get<const Transform>(plants.at(i)).scale.x;
		if (row.growthAmount > 0.0f)
		{
			EXPECT_GT(size, 0.5f) << static_cast<int>(k_Plants.at(i));
		}
		EXPECT_LE(size, 1.0f) << static_cast<int>(k_Plants.at(i));
	}
}

INSTANTIATE_TEST_SUITE_P(Data, PlantsWorld, ::testing::Values(test::world::WorldData::Synthetic, test::world::WorldData::Game),
                         test::world::DataName);
