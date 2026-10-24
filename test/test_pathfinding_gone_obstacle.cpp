/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A circle a villager heads for or goes round is let go of the moment its thing's collide data goes, as in the
// original: the thing leaves the map (chopped, picked up, blown up, deleted), or goes back in to be made again (moved,
// turned, rescaled on the map). The map publishes it (events::CollideDataReleased) and the walk applies the original's
// rule by move state: heading straight on, it looks again from where it stands with the step it has; going round, it
// faces its goal, heads straight on the same way round and looks again; leaving a circle, it does the same and then
// lets go of what it found; any other walk lets go. A thing of many cells that leaves first lets the walkers going round
// the things its circles overlap, through each other, leave them at their next chance. A thing destroyed without
// leaving the map yet is still headed for, as the walker found it, and a thing that waits to be deleted is still an
// obstacle. A slot reused by a new object is not the old one, and a destroyed object still listed in a cell is skipped
// by the looks.
//
// The obstacles are the circles of the things of a map cell's fixed list, from their collide data, in the list's order
// (ecs::thing_circles): a long thing's row of circles, not the one round it, and nothing for a field. The straight line
// heads for the nearest circle it meets, and only the cell its look is for counts: the walker's own when the walk is set
// up, the next step's when the step crosses into another cell. After the things come the cell's water and the land's
// edge and those of its eight neighbours, a 7.2 m circle in each such cell's middle; a walker that reaches one of them
// is a walk to try both ways round.
//
// The same walk's straight line to the goal (STEP_THROUGH) aims its step at the goal again on the original's counter:
// on the first turn of a walk set up straight, 16 turns after an ARRIVED walk finds itself not there, and every 127
// turns after an aim.
//
// The walk's setups are the original's: a walk round obstacles (LINEAR) is aimed, tested for arrival and looks ahead
// when it is set up, not on its first step; the footpath's re-plan looks ahead with the step it has, without a new aim.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <map>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>
#include <spdlog/sinks/null_sink.h>
#include <spdlog/spdlog.h>

#include "Common/EventManager.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Events/MapEvents.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MapCollide.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/PathfindingSystem.h"
#include "ECS/Villager/VillagerScript.h"
#include "Locator.h"
#include "support/LandFakes.h"
#include "support/MapFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// Every point is in the middle of the map and in the middle of a cell's row (cells are 10 m, their middles at 5 m),
// well away from its edges
constexpr float k_Row = 2005.0f;
constexpr float k_Speed = 0.5f;
// The rock ahead of the villager, 8 m east of it: its circle's edge is 6 m ahead, and it is in the villager's map cell
// as well as in the next one east
const glm::vec2 k_TreeCentre {2013.0f, k_Row};
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
		openblack::test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		ecs::map_cells::Clear();
		ecs::object_index::OnLoadMap();
		// each thing's collide data: the shape set for it, else a circle of k_TreeRadius where it stands
		test::FakeMapShapeProvider shapes;
		shapes.meshShape = [this](entt::entity thing, ecs::map_collide::Shape& shape, float& reach) {
			if (const auto set = _shapes.find(thing); set != _shapes.end())
			{
				shape = set->second;
			}
			else
			{
				const auto& position = Reg().Get<const Transform>(thing).position;
				shape = {{position.x, position.z}, k_TreeRadius, {}, 0.0f, "test"};
			}
			// a reach well past the cells' 7.1 m circles, so that those circles alone decide its cells
			reach = shape.radius + 10.0f;
			return true;
		};
		Locator::mapShapeProvider::emplace<test::FakeMapShapeProvider>(shapes);
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
		ecs::map_cells::Clear();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		openblack::test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	ecs::systems::PathfindingSystem& Pathfinding() { return *_pathfinding; }

	/// A thing of many cells on the map at a point, with a circle of k_TreeRadius there as its collide data, or the
	/// shape given: a rock, or a field
	template <typename Kind = MobileStatic>
	entt::entity MakeRock(glm::vec2 at, std::optional<ecs::map_collide::Shape> shape = std::nullopt)
	{
		auto& registry = Reg();
		const auto e = registry.Create();
		ecs::object_index::Assign(e);
		registry.Assign<Transform>(e, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		if constexpr (std::is_same_v<Kind, Field>)
		{
			registry.Assign<Field>(e, -1);
		}
		else
		{
			registry.Assign<Kind>(e);
		}
		if (shape.has_value())
		{
			_shapes[e] = *shape;
		}
		ecs::map_cells::InsertMapObject(e);
		return e;
	}

	/// The rock ahead
	entt::entity MakeTree() { return MakeRock(k_TreeCentre); }

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
		registry.Assign<WallHugObjectReference>(
		    e, WallHugObjectReference {
		           .stepsAway = 5, .entity = obstacle, .circle = 0, .centre = k_TreeCentre, .radius = k_TreeRadius});
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
	std::map<entt::entity, ecs::map_collide::Shape> _shapes;
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
// tree's cell, where the destroyed tree is still listed: the scan skips it and finds no obstacle
TEST_F(PathfindingGoneObstacleTest, LinearDropsADestroyedObstacle)
{
	const auto tree = MakeTree();
	ASSERT_EQ(ecs::map_cells::FirstFixed({201, 200}), tree);
	const auto villager = MakeWalker<MoveStateLinearTag>(2009.8f, tree);
	Reg().Destroy(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
	// it took its step east, into the next cell
	EXPECT_GT(Reg().Get<const Transform>(villager).position.x, 2010.0f);
}

// An orbit round a tree destroyed but not yet out of the map goes on round the circle as it found it. The goal is south
// of the villager, so that the clockwise orbit's next step still turns towards it and the orbit goes on
TEST_F(PathfindingGoneObstacleTest, AnOrbitGoesOnRoundADestroyedThingStillOnTheMap)
{
	const auto tree = MakeTree();
	const auto villager =
	    MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise, glm::vec2(2005.0f, k_Row - 30.0f));
	Reg().Destroy(tree);

	ASSERT_NO_FATAL_FAILURE(Pathfinding().Step(villager));
	ExpectOnlyState(villager, MoveState::Orbit);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(reference->stepsAway, 4);
}

// Going round a circle that goes, the walker faces its goal (south), heads straight on the same way round
// (LINEAR_CW) and looks again from where it stands: the thing is gone, so nothing is ahead
TEST_F(PathfindingGoneObstacleTest, AnOrbitWhoseCircleGoesHeadsStraightOnTheSameWayRound)
{
	const auto tree = MakeTree();
	const auto villager =
	    MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise, glm::vec2(2005.0f, k_Row - 30.0f));
	ecs::map_cells::RemoveMapObject(tree);
	Pathfinding().OnCollideDataReleased(tree, {});

	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_EQ(Reg().Get<const MoveStateLinearTag>(villager).clockwise, MoveStateClockwise::Clockwise);
	const auto& wallHug = Reg().Get<const WallHug>(villager);
	EXPECT_EQ(wallHug.gameAngle, 1536); // due south
	EXPECT_LT(wallHug.stepMetres.y, 0.0f);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// Leaving a circle that goes, the walker does the same, and then lets go of what its look found: the rock ahead
TEST_F(PathfindingGoneObstacleTest, AnExitWhoseCircleGoesLetsGoOfWhatItFindsNext)
{
	const auto gone = MakeRock({2005.0f, k_Row + 5.0f});
	MakeTree();
	const auto villager = MakeWalker<MoveStateExitCircleTag>(2005.0f, gone, MoveStateClockwise::CounterClockwise);
	ecs::map_cells::RemoveMapObject(gone);
	Pathfinding().OnCollideDataReleased(gone, {});

	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_EQ(Reg().Get<const MoveStateLinearTag>(villager).clockwise, MoveStateClockwise::CounterClockwise);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).gameAngle, 0); // due east, at the goal
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// Heading straight on for a circle that goes, the walker looks again from where it stands with the step it has (no new
// aim): the rock behind the one that went, its edge 6 m ahead at 0.5 m a step
TEST_F(PathfindingGoneObstacleTest, AStraightLineWhoseCircleGoesLooksAgainWithTheStepItHas)
{
	const auto gone = MakeRock({2009.0f, k_Row}, ecs::map_collide::Shape {{2009.0f, k_Row}, 1.0f, {}, 0.0f, "gone"});
	const auto behind = MakeTree();
	const auto villager = MakeWalker<MoveStateLinearTag>(2005.0f, gone);
	Reg().Get<WallHug>(villager).gameAngle = 1000; // no aim gives it here
	ecs::map_cells::RemoveMapObject(gone);
	Pathfinding().OnCollideDataReleased(gone, {});

	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_EQ(Reg().Get<const MoveStateLinearTag>(villager).clockwise, MoveStateClockwise::Undefined);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).gameAngle, 1000);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, behind);
	EXPECT_EQ(reference->stepsAway, 12);
}

// Any other walk holding the circle only lets go of it; a walker holding another thing's circle is not touched
TEST_F(PathfindingGoneObstacleTest, AnyOtherWalkOnlyLetsGoAndOtherCirclesStay)
{
	const auto gone = MakeTree();
	const auto other = MakeRock({2005.0f, k_Row + 40.0f});
	const auto straight = MakeWalker<MoveStateStepThroughTag>(2005.0f, gone);
	const auto unrelated = MakeWalker<MoveStateOrbitTag>(2005.0f, other, MoveStateClockwise::Clockwise);
	ecs::map_cells::RemoveMapObject(gone);
	Pathfinding().OnCollideDataReleased(gone, {});

	ExpectOnlyState(straight, MoveState::StepThrough);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(straight));
	ExpectOnlyState(unrelated, MoveState::Orbit);
	ASSERT_TRUE(Reg().AllOf<WallHugObjectReference>(unrelated));
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(unrelated).entity, other);
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(unrelated).stepsAway, 5);
}

// A walker going round with no circle held (it let go of it while its walk was left) faces its goal (south), heads
// straight on the same way round (LINEAR_CW) and looks ahead from where it stands, and takes no step this turn
TEST_F(PathfindingGoneObstacleTest, AnOrbitHoldingNoCircleHeadsStraightOnWithoutAStep)
{
	const auto villager =
	    MakeWalker<MoveStateOrbitTag>(2005.0f, entt::null, MoveStateClockwise::Clockwise, glm::vec2(2005.0f, k_Row - 30.0f));
	Reg().Remove<WallHugObjectReference>(villager);

	Pathfinding().Step(villager);
	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_EQ(Reg().Get<const MoveStateLinearTag>(villager).clockwise, MoveStateClockwise::Clockwise);
	const auto& wallHug = Reg().Get<const WallHug>(villager);
	EXPECT_EQ(wallHug.gameAngle, 1536); // due south
	EXPECT_LT(wallHug.stepMetres.y, 0.0f);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
	const auto& position = Reg().Get<const Transform>(villager).position;
	EXPECT_EQ(position.x, 2005.0f);
	EXPECT_EQ(position.z, k_Row);
}

// Leaving a circle it no longer holds, the walker does the same, the same way round (LINEAR_CCW), and its look finds
// the rock ahead; it still takes no step this turn
TEST_F(PathfindingGoneObstacleTest, AnExitHoldingNoCircleHeadsStraightOnAndLooksAhead)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateExitCircleTag>(2005.0f, entt::null, MoveStateClockwise::CounterClockwise);
	Reg().Remove<WallHugObjectReference>(villager);

	Pathfinding().Step(villager);
	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_EQ(Reg().Get<const MoveStateLinearTag>(villager).clockwise, MoveStateClockwise::CounterClockwise);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).gameAngle, 0); // due east, at the goal
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(Reg().Get<const Transform>(villager).position.x, 2005.0f);
}

// A new object in the destroyed tree's slot (the same index, a new version) is not the tree: when the map lets the
// tree go, a walker going round the newcomer keeps it, and one going round the tree heads straight on
TEST_F(PathfindingGoneObstacleTest, ReusedSlotIsNotTheOldObstacle)
{
	const auto tree = MakeTree();
	const auto villager = MakeWalker<MoveStateOrbitTag>(2005.0f, tree, MoveStateClockwise::Clockwise);
	Reg().Destroy(tree);
	const auto newcomer = MakeTree();
	ASSERT_EQ(entt::to_entity(newcomer), entt::to_entity(tree));
	ASSERT_NE(newcomer, tree);
	const auto other = MakeWalker<MoveStateOrbitTag>(2005.0f, newcomer, MoveStateClockwise::Clockwise);

	Pathfinding().OnCollideDataReleased(tree, {});
	ExpectOnlyState(villager, MoveState::Linear);
	ExpectOnlyState(other, MoveState::Orbit);
	ASSERT_TRUE(Reg().AllOf<WallHugObjectReference>(other));
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(other).entity, newcomer);
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

// ---- the straight walk to the goal (STEP_THROUGH) aims its step again on the counter ---------------------------------

namespace
{
/// The goal 1000 m from the walker, a little off east by the turn: every aim at it gives another game angle than the
/// aims a few turns before and after
glm::vec2 TurningGoal(const Transform& transform, int turn)
{
	const float off = static_cast<float>(turn % 7 - 3) * 0.05f;
	return glm::vec2(transform.position.x, transform.position.z) + glm::vec2(1.0f, off) * 1000.0f;
}

/// A villager at x on the row, walking straight east in the move state Tag, with the step-rebuild counter at `counter`
template <typename Tag>
entt::entity MakeStraightWalker(ecs::Registry& registry, float x, int8_t counter)
{
	const auto e = registry.Create();
	registry.Assign<Transform>(e, glm::vec3(x, 0.0f, k_Row), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& wallHug = registry.Assign<WallHug>(e, k_Goal, glm::vec2(k_Speed, 0.0f), 0.0f, k_Speed);
	wallHug.gameAngle = 1000; // no aim gives it here
	wallHug.turnsUntilStepRebuild = counter;
	registry.Assign<Tag>(e, MoveStateClockwise::Undefined, glm::vec2(x, k_Row));
	return e;
}

/// The turns (from 1) on which the walker's step was aimed again, out of `turns` turns towards the turning goal
std::vector<int> AimedTurns(ecs::Registry& registry, ecs::systems::PathfindingSystem& pathfinding, entt::entity walker,
                            int turns)
{
	std::vector<int> aimed;
	for (int turn = 1; turn <= turns; ++turn)
	{
		auto& wallHug = registry.Get<WallHug>(walker);
		wallHug.goal = TurningGoal(registry.Get<const Transform>(walker), turn);
		const auto before = wallHug.gameAngle;
		pathfinding.Step(walker);
		if (registry.Get<const WallHug>(walker).gameAngle != before)
		{
			aimed.push_back(turn);
		}
	}
	return aimed;
}
} // namespace

// A straight walk as SetupMobileMoveToPos leaves it (the counter at 1) aims its step on its first turn, then every 127
// turns: on turns 1, 128 and 255 of 300, and on no other
TEST_F(PathfindingGoneObstacleTest, StepThroughAimsOnTheFirstTurnThenEvery127Turns)
{
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 1500.0f, 1);
	EXPECT_EQ(AimedTurns(Reg(), Pathfinding(), villager, 300), (std::vector<int> {1, 128, 255}));
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).turnsUntilStepRebuild, 127 - (300 - 255));
}

// An ARRIVED walk that is not there goes on straight with the counter at 16, and counts its first turn at once: its
// step is aimed again on its 16th turn
TEST_F(PathfindingGoneObstacleTest, ArrivedNotThereAimsAgainOnItsSixteenthTurn)
{
	const auto villager = MakeStraightWalker<MoveStateArrivedTag>(Reg(), 1500.0f, 0);
	Pathfinding().Step(villager);
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).turnsUntilStepRebuild, 15);
	EXPECT_EQ(AimedTurns(Reg(), Pathfinding(), villager, 20), (std::vector<int> {15}));
}

// ---- the walk's setups ------------------------------------------------------------------------------------------------

// A walk round obstacles looks ahead when it is set up: aimed east at the goal, the rock's edge 6 m ahead is 12 steps
// away (6 / 0.498 m) before the walker has taken a step
TEST_F(PathfindingGoneObstacleTest, LinearSetupAimsAndLooksAheadAtOnce)
{
	const auto tree = MakeTree();
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::Linear);
	ExpectOnlyState(villager, MoveState::Linear);
	const auto& wallHug = Reg().Get<const WallHug>(villager);
	EXPECT_EQ(wallHug.gameAngle, 0);
	EXPECT_EQ(wallHug.step, glm::ivec2(3264, 0));
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(reference->circle, 0);
	EXPECT_EQ(reference->centre, k_TreeCentre);
	EXPECT_EQ(reference->radius, k_TreeRadius);
	EXPECT_EQ(reference->stepsAway, 12);
}

// A goal within a step is there at once: ARRIVED, with no look ahead
TEST_F(PathfindingGoneObstacleTest, LinearSetupWithinAStepArrives)
{
	MakeTree();
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, glm::vec2(2005.3f, k_Row), MoveState::Linear);
	ExpectOnlyState(villager, MoveState::Arrived);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// A straight walk set up with the state aims its step again on its first turn
TEST_F(PathfindingGoneObstacleTest, StepThroughSetupAimsOnTheFirstTurn)
{
	const auto villager = MakeStraightWalker<MoveStateLinearTag>(Reg(), 1500.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::StepThrough);
	ExpectOnlyState(villager, MoveState::StepThrough);
	EXPECT_EQ(Reg().Get<const WallHug>(villager).turnsUntilStepRebuild, 1);
}

// The footpath's re-plan makes the walk LINEAR and looks ahead with the step it has: no new aim, the goal kept
TEST_F(PathfindingGoneObstacleTest, FootpathReplanLooksAheadWithTheStepItHas)
{
	const auto tree = MakeTree();
	const auto villager = MakeStraightWalker<MoveStateOrbitTag>(Reg(), 2005.0f, 0);
	ecs::living_footpath::RePlanHug(villager);
	ExpectOnlyState(villager, MoveState::Linear);
	const auto& wallHug = Reg().Get<const WallHug>(villager);
	EXPECT_EQ(wallHug.gameAngle, 1000);
	EXPECT_EQ(wallHug.stepMetres, glm::vec2(k_Speed, 0.0f));
	EXPECT_EQ(wallHug.goal, k_Goal);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, tree);
	EXPECT_EQ(reference->stepsAway, 12);
}

// ---- what the straight line comes across: the circles of the cell's things ------------------------------------------

// The nearest circle on the line wins, not the first of the cell's list: the far rock went on the map last, so it heads
// the list. The near one's edge is 3 m ahead: 6 steps
TEST_F(PathfindingGoneObstacleTest, TheLineHeadsForTheNearestCircleNotTheFirstOfTheList)
{
	const auto nearRock = MakeRock({2009.0f, k_Row}, ecs::map_collide::Shape {{2009.0f, k_Row}, 1.0f, {}, 0.0f, "near"});
	const auto farRock = MakeTree();
	ASSERT_EQ(ecs::map_cells::FirstFixed({200, 200}), farRock);
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::Linear);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, nearRock);
	EXPECT_EQ(reference->radius, 1.0f);
	EXPECT_EQ(reference->stepsAway, 6);
}

// A long thing stands in the way with its row of circles, not with the circle round the whole of it: the line meets the
// middle one of the row (its edge 6 m ahead, 12 steps), where the circle round the row would have been 4 m ahead
TEST_F(PathfindingGoneObstacleTest, TheLineMeetsTheCircleOfALongThingsRow)
{
	const ecs::map_collide::Shape row {
	    {2013.0f, k_Row}, 4.0f, {{2012.0f, k_Row - 3.0f}, {2012.0f, k_Row}, {2012.0f, k_Row + 3.0f}}, 1.0f, "row"};
	const auto wall = MakeRock({2013.0f, k_Row}, row);
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::Linear);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, wall);
	EXPECT_EQ(reference->circle, 1);
	EXPECT_EQ(reference->centre, glm::vec2(2012.0f, k_Row));
	EXPECT_EQ(reference->radius, 1.0f);
	EXPECT_EQ(reference->stepsAway, 12);
}

// A field is walked over: its collide data is no obstacle
TEST_F(PathfindingGoneObstacleTest, AFieldIsWalkedOver)
{
	MakeRock<Field>(k_TreeCentre);
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::Linear);
	ExpectOnlyState(villager, MoveState::Linear);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(villager));
}

// Only the cell the look is for counts. A rock in the next cell east only is not seen from the walker's own cell when
// the walk is set up; once the step crosses into that cell, the look from where the walker stands finds it, 6.2 m
// ahead (12 steps), and the turn counts one down at once
TEST_F(PathfindingGoneObstacleTest, OnlyTheCellOfTheLookCounts)
{
	const auto rock = MakeRock({2018.0f, k_Row});
	ASSERT_EQ(ecs::map_cells::FirstFixed({200, 200}), entt::entity {entt::null});
	ASSERT_EQ(ecs::map_cells::FirstFixed({201, 200}), rock);
	const auto setUp = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(setUp, k_Goal, MoveState::Linear);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(setUp));

	const auto walker = MakeStraightWalker<MoveStateLinearTag>(Reg(), 2009.8f, 0);
	Pathfinding().Step(walker);
	ExpectOnlyState(walker, MoveState::Linear);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(walker);
	ASSERT_NE(reference, nullptr);
	EXPECT_EQ(reference->entity, rock);
	EXPECT_EQ(reference->stepsAway, 11);
}

// ---- water and the land's edge ----------------------------------------------------------------------------------------

namespace
{
/// Land everywhere but for the cell east of the walker's, which is water
openblack::test::WaterCellIsland& WaterEastOfTheWalker()
{
	Locator::terrainSystem::reset();
	auto& island = static_cast<openblack::test::WaterCellIsland&>(
	    Locator::terrainSystem::emplace<openblack::test::WaterCellIsland>(static_cast<uint16_t>(32)));
	island.Cell({201, 200}).properties.hasWater = 1;
	return island;
}
} // namespace

// The water in the next cell east stands in the way as a 7.2 m circle in its middle (2015 m): its edge 2.8 m ahead, 5
// steps. It is no thing's: the walker keeps the cell
TEST_F(PathfindingGoneObstacleTest, TheLineHeadsForTheWaterOfANeighbouringCell)
{
	WaterEastOfTheWalker();
	const auto villager = MakeStraightWalker<MoveStateStepThroughTag>(Reg(), 2005.0f, 0);
	ecs::villager::SetupMobileMoveToPos(villager, k_Goal, MoveState::Linear);
	const auto* reference = Reg().TryGet<const WallHugObjectReference>(villager);
	ASSERT_NE(reference, nullptr);
	EXPECT_TRUE(reference->landscape);
	EXPECT_TRUE(reference->landscapeOrFence);
	EXPECT_EQ(reference->entity, entt::entity {entt::null});
	EXPECT_EQ(reference->cell, glm::ivec2(201, 200));
	EXPECT_EQ(reference->centre, glm::vec2(2015.0f, k_Row));
	EXPECT_EQ(reference->radius, 7.2f);
	EXPECT_EQ(reference->stepsAway, 5);
}

// Reaching the water's circle, the walker goes round it, and its walk is one to try both ways round; reaching a thing's
// circle, it is not
TEST_F(PathfindingGoneObstacleTest, AWalkerThatReachesWaterIsOneToTryBothWaysRound)
{
	WaterEastOfTheWalker();
	const auto villager = MakeStraightWalker<MoveStateLinearTag>(Reg(), 2005.0f, 0);
	Reg().Assign<WallHugObjectReference>(villager, WallHugObjectReference {.stepsAway = 0,
	                                                                       .entity = entt::null,
	                                                                       .circle = 0,
	                                                                       .centre = {2015.0f, k_Row},
	                                                                       .radius = 7.2f,
	                                                                       .landscape = true,
	                                                                       .cell = {201, 200},
	                                                                       .landscapeOrFence = true});
	Pathfinding().Step(villager);
	ExpectOnlyState(villager, MoveState::Orbit);
	EXPECT_TRUE(Reg().Get<const WallHug>(villager).needsLookahead);
	ASSERT_TRUE(Reg().AllOf<WallHugObjectReference>(villager));
	EXPECT_TRUE(Reg().Get<const WallHugObjectReference>(villager).landscape);

	// the next turn's walk hands it over to nothing: the flag is the turn's
	Pathfinding().Step(villager);
	EXPECT_FALSE(Reg().Get<const WallHug>(villager).needsLookahead);
}

TEST_F(PathfindingGoneObstacleTest, AWalkerThatReachesAThingIsNotOneToTryBothWaysRound)
{
	const auto rock = MakeRock({2005.0f, k_Row + 40.0f});
	const auto walker = MakeWalker<MoveStateLinearTag>(2005.0f, rock);
	Reg().Get<WallHugObjectReference>(walker).stepsAway = 0;
	Pathfinding().Step(walker);
	ExpectOnlyState(walker, MoveState::Orbit);
	EXPECT_FALSE(Reg().Get<const WallHug>(walker).needsLookahead);
}

// ---- the map lets a thing's circles go --------------------------------------------------------------------------------

// The map tells when a thing's collide data goes: when it leaves the map, and when it moves on the map (out and back in,
// its collide data made again). A thing without collide data (a pot) tells nothing
TEST_F(PathfindingGoneObstacleTest, TheMapTellsWhenAThingsCollideDataGoes)
{
	auto& events = Locator::events::emplace<EventManager>();
	std::vector<entt::entity> released;
	events.AddHandler<ecs::events::CollideDataReleased>(
	    [&released](const ecs::events::CollideDataReleased& event) { released.push_back(event.thing); });
	const auto rock = MakeTree();
	const auto tree = MakeRock<Tree>({2005.0f, k_Row + 20.0f});
	const auto pot = MakeRock<Pot>({2005.0f, k_Row + 30.0f});

	ecs::map_cells::MoveMapObject(rock, glm::vec3(2014.0f, 0.0f, k_Row));
	ecs::map_cells::RemoveMapObject(tree);
	ecs::map_cells::RemoveMapObject(pot);
	Locator::events::reset();
	EXPECT_EQ(released, (std::vector<entt::entity> {rock, tree}));
}

// A thing of many cells that leaves the map names the things of many cells its circles overlap, through each other,
// looked for in the cells of each: B overlaps A, C overlaps B only (and is in one of A's cells too), D is in A's cell
// without overlapping any, and a tree overlapping A is not of many cells. The walkers going round B or C may leave them
// at their next chance; the one going round D is untouched; the one going round A lets go
TEST_F(PathfindingGoneObstacleTest, AThingOfManyCellsThatGoesLetsItsOverlappingNeighboursBeLeft)
{
	auto& events = Locator::events::emplace<EventManager>();
	std::vector<entt::entity> overlapping;
	events.AddHandler<ecs::events::CollideDataReleased>(
	    [&overlapping](const ecs::events::CollideDataReleased& event) { overlapping = event.overlapping; });
	const auto a = MakeRock({2013.0f, k_Row}, ecs::map_collide::Shape {{2013.0f, k_Row}, 2.0f, {}, 0.0f, "a"});
	const auto b = MakeRock({2017.0f, k_Row}, ecs::map_collide::Shape {{2017.0f, k_Row}, 3.0f, {}, 0.0f, "b"});
	const auto c = MakeRock({2022.5f, k_Row}, ecs::map_collide::Shape {{2022.5f, k_Row}, 3.0f, {}, 0.0f, "c"});
	const auto d = MakeRock({2005.0f, k_Row + 5.0f}, ecs::map_collide::Shape {{2005.0f, k_Row + 5.0f}, 1.0f, {}, 0.0f, "d"});
	MakeRock<Tree>({2012.0f, k_Row});
	const auto aroundA = MakeWalker<MoveStateOrbitTag>(2005.0f, a, MoveStateClockwise::Clockwise);
	const auto aroundB = MakeWalker<MoveStateOrbitTag>(2005.0f, b, MoveStateClockwise::Clockwise);
	const auto aroundC = MakeWalker<MoveStateOrbitTag>(2005.0f, c, MoveStateClockwise::Clockwise);
	const auto aroundD = MakeWalker<MoveStateOrbitTag>(2005.0f, d, MoveStateClockwise::Clockwise);

	ecs::map_cells::RemoveMapObject(a);
	Locator::events::reset();
	EXPECT_EQ(overlapping, (std::vector<entt::entity> {a, b, c}));

	Pathfinding().OnCollideDataReleased(a, overlapping);
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(aroundA));
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(aroundB).entryDistance, 0x7FFFFFFFU);
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(aroundC).entryDistance, 0x7FFFFFFFU);
	EXPECT_EQ(Reg().Get<const WallHugObjectReference>(aroundD).entryDistance, 0U);
	ExpectOnlyState(aroundB, MoveState::Orbit);
}
