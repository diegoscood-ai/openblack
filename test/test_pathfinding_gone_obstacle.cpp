/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A wall-hug obstacle that is gone is no obstacle: openblack's own guard in the villagers' walk (the original reads the
// object without looking). A tree chopped, blown up or thrown into a store while a villager walks towards it or round
// it must not be read again. At the start of the villager's step a reference to an object that is destroyed or without
// a footprint is dropped: a LINEAR walk forgets it and goes on, an orbit or the exit from one is abandoned
// (STEP_THROUGH, straight to the goal). A slot reused by a new object is not the old obstacle, and an object filed in an
// obstacle cell and destroyed since is skipped by the scans. With the obstacle still there the walk is unchanged, even
// when it waits to be deleted, as in the original.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/Fixed.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// Every point is in the middle of the map and in the middle of a cell's row (cells are 10 m, their middles at 5 m),
// well away from its edges
constexpr float k_Row = 2005.0f;
constexpr float k_Speed = 0.5f;
// The tree ahead of the villager, in the middle of the cell east of the villager's
const glm::vec2 k_TreeCentre {2015.0f, k_Row};
constexpr float k_TreeRadius = 2.0f;
// The villager walks east, to a goal far beyond the tree
const glm::vec2 k_Goal {2100.0f, k_Row};

class PathfindingGoneObstacleTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		for (const auto* name : {"ai", "pathfinding", "game"})
		{
			if (spdlog::get(name) == nullptr)
			{
				spdlog::create<spdlog::sinks::null_sink_mt>(name);
			}
		}
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		// flat land: the steps land at altitude 0
		Locator::terrainSystem::emplace<openblack::test::HeightFieldIsland>([](glm::vec2) { return 0.0f; });
		// the system under test, injected where the walk's scans look it up
		_pathfinding = &static_cast<ecs::systems::PathfindingSystem&>(
		    Locator::pathfindingSystem::emplace<ecs::systems::PathfindingSystem>());
	}

	void TearDown() override
	{
		_pathfinding = nullptr;
		Locator::pathfindingSystem::reset();
		Locator::terrainSystem::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	ecs::systems::PathfindingSystem& Pathfinding() { return *_pathfinding; }

	/// A tree: a footprint (Fixed) where it stands
	static entt::entity MakeTree()
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(k_TreeCentre.x, 0.0f, k_TreeCentre.y), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Fixed>(e, k_TreeCentre, k_TreeRadius);
		return e;
	}

	/// A villager at x on the row, facing east at k_Speed, in the move state Tag, hugging (or heading for) the obstacle
	template <typename Tag>
	static entt::entity MakeWalker(float x, entt::entity obstacle, MoveStateClockwise clockwise = MoveStateClockwise::Undefined,
	                               glm::vec2 goal = k_Goal)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		registry.Assign<Transform>(e, glm::vec3(x, 0.0f, k_Row), glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<WallHug>(e, goal, glm::vec2(k_Speed, 0.0f), 0.0f, k_Speed);
		registry.Assign<Tag>(e, clockwise, glm::vec2(x, k_Row));
		registry.Assign<WallHugObjectReference>(e, static_cast<uint8_t>(5), obstacle);
		return e;
	}

	/// The move state tags the walker has, as one per state
	static void ExpectOnlyState(entt::entity e, MoveState state)
	{
		auto& registry = Reg();
		EXPECT_EQ(registry.AllOf<MoveStateLinearTag>(e), state == MoveState::Linear);
		EXPECT_EQ(registry.AllOf<MoveStateOrbitTag>(e), state == MoveState::Orbit);
		EXPECT_EQ(registry.AllOf<MoveStateExitCircleTag>(e), state == MoveState::ExitCircle);
		EXPECT_EQ(registry.AllOf<MoveStateStepThroughTag>(e), state == MoveState::StepThrough);
		EXPECT_EQ(registry.AllOf<MoveStateFinalStepTag>(e), state == MoveState::FinalStep);
		EXPECT_EQ(registry.AllOf<MoveStateArrivedTag>(e), state == MoveState::Arrived);
	}

private:
	ecs::systems::PathfindingSystem* _pathfinding = nullptr;
};
} // namespace

// With the tree alive the walk is unchanged: the reference stays and counts down one step
TEST_F(PathfindingGoneObstacleTest, LinearKeepsALiveObstacle)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateLinearTag>(2005.0f, tree);
	Pathfinding().Step(villager);
	ExpectOnlyState(villager, MoveState::Linear);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(reference->stepsAway, 4);
}

// A LINEAR walk towards a tree destroyed since its last step forgets the tree and walks on. The step crosses into the
// tree's cell, where the destroyed tree is still filed: the scan skips it and finds no obstacle
TEST_F(PathfindingGoneObstacleTest, LinearDropsADestroyedObstacle)
{
	const auto tree = MakeTree();
	Pathfinding().FileObstacles();
	ASSERT_TRUE(Pathfinding().ObstaclesIn(ecs::MapInterface::GetGridCell(k_TreeCentre)).contains(tree));
	const auto villager = MakeWalker<MoveStateLinearTag>(2009.8f, tree);
	Reg().Destroy(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
	// it took its step east, into the next cell
	EXPECT_GT(Reg().Get<const Transform>(villager).position.x, 2010.0f);
}

// An orbit round a tree destroyed since its last step has nothing to hug: the move is abandoned, straight on to the
// goal
TEST_F(PathfindingGoneObstacleTest, OrbitAbandonsADestroyedObstacle)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise);
	Reg().Destroy(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// The exit from an orbit round a tree destroyed since its last step is abandoned the same way
TEST_F(PathfindingGoneObstacleTest, ExitCircleAbandonsADestroyedObstacle)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateExitCircleTag>(2005.0f, tree, MoveStateClockwise::Clockwise);
	Reg().Destroy(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// A new object in the destroyed tree's slot (the same index, a new version) is not the tree: the reference to the tree
// is still dropped, even though the new object has a footprint of its own
TEST_F(PathfindingGoneObstacleTest, ReusedSlotIsNotTheOldObstacle)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise);
	Reg().Destroy(tree);
	const auto newcomer = MakeTree();
	ASSERT_EQ(entt::to_entity(newcomer), entt::to_entity(tree));
	ASSERT_NE(newcomer, tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
	EXPECT_TRUE(Reg().Valid(newcomer));
}

// A tree waiting to be deleted (Unavailable) but still there is still an obstacle, as in the original: an orbit round
// it keeps hugging it. The goal is south of the villager, so that the clockwise orbit's next step still turns towards
// it and the orbit goes on
TEST_F(PathfindingGoneObstacleTest, UnavailableObstacleIsKept)
{
	const auto tree = MakeTree();
	const auto villager =
	    MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise, glm::vec2(2005.0f, k_Row - 30.0f));
	Reg().Assign<Unavailable>(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::Orbit);
	EXPECT_EQ(Reg().Get<const MoveStateOrbitTag>(villager).clockwise, MoveStateClockwise::Clockwise);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(reference->stepsAway, 4);
}
