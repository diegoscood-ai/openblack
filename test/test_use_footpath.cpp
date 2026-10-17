/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// UseFootpathIfNecessary, on hand-made footpaths and a villager with a fake state table (test_villager_core's): the
// nearest footpath within 40 m is taken; with none, and no town, the walk falls back to SetupMoveToWithHug. And the
// walk along the footpath itself (MOVE_ON_PATH's turn and its exit), with the synced stream's draws faked.

#define LOCATOR_IMPLEMENTATIONS

#include <algorithm>
#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandomTesting.h"
#include "Common/RandomNumberManager.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Footpaths.h"
#include "ECS/LivingFootpath.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Villager/VillagerCore.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/LivingActionFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace living_footpath = openblack::ecs::living_footpath;
namespace map_coords = openblack::map_coords;
namespace villager = openblack::ecs::villager;
using Index = LivingAction::Index;

namespace
{
class TestRng final: public RandomNumberManagerInterface
{
	std::mt19937 _generator {1};
	std::mt19937& Generator() override { return _generator; }
	uint32_t _runSeed {0};
	[[nodiscard]] uint32_t GetRunSeed() const override { return _runSeed; }
	void SetRunSeed(uint32_t seed) override { _runSeed = seed; }
	[[nodiscard]] std::mt19937& Stream(RandomStream /*stream*/) override { return _generator; }
	std::optional<std::reference_wrapper<std::mutex>> LockAccess() override { return std::nullopt; }
};

/// Every state entry / exit succeeds and nothing runs (test_villager_core's FakeStateTable, without its log)
class QuietStateTable final: public test::LivingActionFakeBase
{
public:
	void Update() override {}
	[[nodiscard]] VillagerStates VillagerGetState(const LivingAction& action, Index index) const override
	{
		return static_cast<VillagerStates>(action.states.at(static_cast<size_t>(index)));
	}
	[[nodiscard]] VillagerStates VillagerGetFinalState(const LivingAction& /*action*/) const override
	{
		return VillagerStates::InvalidState;
	}
	void VillagerSetState(LivingAction&, Index, VillagerStates, bool) const override {}
	uint32_t VillagerCallState(LivingAction&, Index) const override { return 0; }
	uint32_t VillagerCallEntryState(LivingAction&, VillagerStates, VillagerStates, VillagerStates) const override { return 1; }
	uint32_t VillagerCallExitState(LivingAction&, VillagerStates, VillagerStates) const override { return 1; }
	int VillagerCallOutOfAnimation(LivingAction&, Index) const override { return -1; }
	bool VillagerCallValidate(LivingAction&, Index) const override { return false; }
};

class UseFootpathTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		auto info = std::make_unique<InfoConstants>();
		info->villager.at(0).life = 1.0f;
		info->villager.at(0).moveState = LivingStates::LivingMoveToPos;
		Locator::infoConstants::reset(info.release());
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		Locator::rng::emplace<TestRng>();
		Locator::livingActionSystem::emplace<QuietStateTable>();
		game_clock::SetTurn(100);
	}
	void TearDown() override
	{
		Locator::livingActionSystem::reset();
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
		Locator::infoConstants::reset();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	/// A footpath along x, one visible node every 10 m from x0 (test_footpaths' Make)
	static entt::entity Make(int x0, int count)
	{
		const auto e = Reg().Create();
		auto& f = Reg().Assign<Footpath>(e);
		for (int i = 0, x = x0; i < count; ++i, x += 10)
		{
			const auto coords = map_coords::FromMetres(glm::vec2(static_cast<float>(x), 0.0f));
			f.nodes.push_back({glm::vec3(static_cast<float>(x), 0.0f, 0.0f), coords, 1, f.nextNodeId++});
		}
		return e;
	}
	static entt::entity Link(std::vector<entt::entity> paths)
	{
		std::vector<Footpath::Id> ids;
		for (const auto p : paths)
		{
			ids.push_back(static_cast<Footpath::Id>(p));
		}
		const auto link = Reg().Create();
		Reg().Assign<FootpathLink>(link, glm::vec3(0.0f), ids);
		return link;
	}
	static entt::entity MakeVillager(float x)
	{
		const auto e = Reg().Create();
		auto& v = Reg().Assign<Villager>(e);
		v.life = 1.0f;
		v.town = entt::null;
		v.abode = entt::null;
		v.food = 1.0f;
		Reg().Assign<LivingAction>(e, static_cast<VillagerStates>(163), static_cast<uint16_t>(0));
		Reg().Assign<Transform>(e, glm::vec3(x, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
		// the walk's goal (WallHug), as test_villager_core's SetupMoveToWithHug cases
		Reg().Assign<WallHug>(e, glm::vec2(), glm::vec2(), 0.0f, 0.1f);
		return e;
	}
	static map_coords::MapCoords At(float x) { return map_coords::FromMetres(glm::vec2(x, 0.0f)); }

	/// The villager walking the footpath from its head (toward its last node), on the head node, with `final` to come
	static void PutOnHead(entt::entity living, entt::entity path, VillagerStates final)
	{
		auto& action = Reg().Get<LivingAction>(living);
		action.states.at(static_cast<size_t>(Index::Top)) = static_cast<uint8_t>(VillagerStates::MoveOnPath);
		action.states.at(static_cast<size_t>(Index::Final)) = static_cast<uint8_t>(final);
		auto& walk = Reg().Assign<LivingFootpath>(living);
		walk.footpath = path;
		walk.node = footpaths::HeadNode(path);
		walk.reverse = false;
		footpaths::AddOccupant(path, walk.node, living);
	}
	static const std::vector<entt::entity>& Occupants(entt::entity path, size_t index)
	{
		return Reg().Get<const Footpath>(path).nodes.at(index).occupants;
	}
	static VillagerStates Top(entt::entity living)
	{
		return static_cast<VillagerStates>(Reg().Get<const LivingAction>(living).states.at(static_cast<size_t>(Index::Top)));
	}
};

TEST_F(UseFootpathTest, TakesTheNearestFootpathWithinFortyMetres)
{
	const auto near = Make(0, 3);
	const auto far = Make(200, 3);
	const auto link = Link({far, near});
	const auto living = MakeVillager(1.0f);
	// GetNearestPathTo(the link's footpaths, pos, 40) and SetupMoveOnFootpath != 0 -> 1
	EXPECT_EQ(footpaths::UseFootpathIfNecessary(link, living, At(50.0f), 0, entt::null), 1u);
	const auto* fp = Reg().TryGet<LivingFootpath>(living);
	ASSERT_NE(fp, nullptr);
	EXPECT_TRUE(fp->footpath == near);
	EXPECT_NE(fp->node, footpaths::k_NoNode);
	// SetupMobileMoveToPos(the node, 12): the goal is that node, on the footpath at x 0..20
	const auto& hug = Reg().Get<WallHug>(living);
	EXPECT_LE(hug.goal.x, 20.5f);
	EXPECT_NEAR(hug.goal.y, 0.0f, 0.01f);
}

TEST_F(UseFootpathTest, NoFootpathAndNoTownFallsBackToTheHug)
{
	const auto far = Make(500, 3);
	const auto link = Link({far});
	const auto living = MakeVillager(1.0f);
	// nothing within 40 m, no owner and no town -> SetupMoveToWithHug(pos, final): the footpath fields are not
	// touched
	static_cast<void>(footpaths::UseFootpathIfNecessary(link, living, At(50.0f), 0, entt::null));
	const auto* fp = Reg().TryGet<LivingFootpath>(living);
	EXPECT_TRUE(fp == nullptr || fp->footpath == entt::null);
	// the hug's goal is pos itself
	const auto& hug = Reg().Get<WallHug>(living);
	EXPECT_NEAR(hug.goal.x, 50.0f, 0.01f);
	EXPECT_NEAR(hug.goal.y, 0.0f, 0.01f);
}

TEST_F(UseFootpathTest, MoveOnFootpathWalksNodeByNodeToTheFinalState)
{
	// the synced stream's draws, in order: x then z of the goal on the inner node
	game_random::testing::ScopedState streams;
	constexpr std::array<uint32_t, 2> k_Draws = {0x1000 + 0x100, 0x1000 - 0x200};
	size_t draws = 0;
	game_random::testing::SetGameRand(
	    [&draws, &k_Draws](uint32_t n) {
		    EXPECT_EQ(n, 0x2000u);
		    return draws < k_Draws.size() ? k_Draws.at(draws++) : 0x1000u;
	    },
	    nullptr);
	const auto path = Make(0, 3);
	const auto living = MakeVillager(0.0f);
	PutOnHead(living, path, VillagerStates::DecideWhatToDo);
	const auto& nodes = Reg().Get<const Footpath>(path).nodes;
	const auto& walk = Reg().Get<const LivingFootpath>(living);

	// still on the way: the walk's result, nothing else
	EXPECT_EQ(living_footpath::MoveOnFootpath(living, 1), 1u);
	EXPECT_EQ(walk.node, nodes.at(0).id);
	EXPECT_EQ(draws, 0u);

	// at the head: on to the inner node, whose goal is moved off it by the two draws, x first
	EXPECT_EQ(living_footpath::MoveOnFootpath(living, 0xA), 1u);
	EXPECT_EQ(walk.node, nodes.at(1).id);
	EXPECT_TRUE(Occupants(path, 0).empty());
	ASSERT_EQ(Occupants(path, 1).size(), 1u);
	EXPECT_TRUE(Occupants(path, 1).front() == living);
	EXPECT_EQ(draws, 2u);
	const auto& hug = Reg().Get<const WallHug>(living);
	EXPECT_FLOAT_EQ(hug.goal.x, map_coords::ToMetres(nodes.at(1).coords.x + 0x100));
	EXPECT_FLOAT_EQ(hug.goal.y, map_coords::ToMetres(nodes.at(1).coords.z - 0x200));
	// a straight walk, not round obstacles
	EXPECT_TRUE(Reg().AllOf<MoveStateStepThroughTag>(living));

	// on to the last node: no node beyond it, so no draw and the goal is the node itself
	EXPECT_EQ(living_footpath::MoveOnFootpath(living, 0xA), 1u);
	EXPECT_EQ(walk.node, nodes.at(2).id);
	EXPECT_TRUE(Occupants(path, 1).empty());
	EXPECT_EQ(Occupants(path, 2).size(), 1u);
	EXPECT_EQ(draws, 2u);
	EXPECT_FLOAT_EQ(hug.goal.x, map_coords::ToMetres(nodes.at(2).coords.x));
	EXPECT_FLOAT_EQ(hug.goal.y, map_coords::ToMetres(nodes.at(2).coords.z));

	// nothing left: no node, out of the last one, the final state, and "arrived"
	EXPECT_EQ(living_footpath::MoveOnFootpath(living, 0xA), 0xAu);
	EXPECT_EQ(walk.node, footpaths::k_NoNode);
	EXPECT_TRUE(walk.footpath == path);
	EXPECT_TRUE(Occupants(path, 2).empty());
	EXPECT_EQ(Top(living), VillagerStates::DecideWhatToDo);
	EXPECT_EQ(draws, 2u);
}

TEST_F(UseFootpathTest, MoveOnFootpathTakesTheNextFootpathFromItsNearerEnd)
{
	game_random::testing::ScopedState streams;
	size_t draws = 0;
	game_random::testing::SetGameRand(
	    [&draws](uint32_t /*n*/) {
		    ++draws;
		    return 0x1000u;
	    },
	    nullptr);
	// a one-node footpath, then one whose last node (x 40) is nearer the villager than its head (x 60)
	const auto first = Make(40, 1);
	const auto next = Make(40, 3);
	auto& nextNodes = Reg().Get<Footpath>(next).nodes;
	std::reverse(nextNodes.begin(), nextNodes.end());
	const auto living = MakeVillager(40.0f);
	PutOnHead(living, first, VillagerStates::DecideWhatToDo);
	living_footpath::SetNextFootpath(living, next);
	const auto& walk = Reg().Get<const LivingFootpath>(living);

	// the end of the first footpath: the next one, toward its head, one node in from its last (x 50)
	EXPECT_EQ(living_footpath::MoveOnFootpath(living, 0xA), 1u);
	EXPECT_TRUE(walk.footpath == next);
	EXPECT_TRUE(walk.next == entt::null);
	EXPECT_TRUE(walk.reverse);
	EXPECT_EQ(walk.node, nextNodes.at(1).id);
	EXPECT_TRUE(Occupants(first, 0).empty());
	EXPECT_EQ(Occupants(next, 1).size(), 1u);
	// the goal is that node exactly: no draw
	EXPECT_EQ(draws, 0u);
	const auto& hug = Reg().Get<const WallHug>(living);
	EXPECT_FLOAT_EQ(hug.goal.x, map_coords::ToMetres(nextNodes.at(1).coords.x));
	EXPECT_EQ(Top(living), VillagerStates::MoveOnPath);
}

TEST_F(UseFootpathTest, ExitMoveOnPathLeavesTheNodeAndKeepsTheFootpath)
{
	const auto path = Make(0, 3);
	const auto living = MakeVillager(0.0f);
	PutOnHead(living, path, VillagerStates::DecideWhatToDo);
	// on the node twice: every entry goes
	footpaths::AddOccupant(path, footpaths::HeadNode(path), living);
	Reg().Assign<WallHugObjectReference>(living, static_cast<uint8_t>(0), path);
	EXPECT_EQ(living_footpath::ExitMoveOnPath(living), 1u);
	const auto& walk = Reg().Get<const LivingFootpath>(living);
	EXPECT_EQ(walk.node, footpaths::k_NoNode);
	EXPECT_TRUE(walk.footpath == path);
	EXPECT_TRUE(Occupants(path, 0).empty());
	EXPECT_FALSE(Reg().AllOf<WallHugObjectReference>(living));
}

TEST_F(UseFootpathTest, ABigForestsArrivePointForAWalkIsItsDoor)
{
	// no mesh, so no door point: the door is the forest's position
	const auto forest = Reg().Create();
	Reg().Assign<BigForest>(forest);
	Reg().Assign<Transform>(forest, glm::vec3(30.0f, 0.0f, 40.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto arrive = living_footpath::GetArrivePos(forest);
	const auto at = living_footpath::PosOf(forest);
	EXPECT_EQ(arrive.x, at.x);
	EXPECT_EQ(arrive.z, at.z);
	EXPECT_EQ(arrive.altitude, 0.0f);
}
} // namespace
