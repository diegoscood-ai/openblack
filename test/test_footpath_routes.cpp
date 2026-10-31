/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Footpaths made by the route planner: ConvertCreaturePlanToFootpath between two nodes, on a plan
// round one hand-made circle (no world), and the footpath finder that plans one over several turns.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>

#include <glm/mat3x3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/FootpathFinder.h"
#include "ECS/Components/Transform.h"
#include "ECS/Footpaths.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RoutePlan.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace footpaths = openblack::ecs::footpaths;
namespace route_plan_world = openblack::ecs::route_plan_world;

namespace
{
class FootpathRoutesTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		route_planner::ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}
	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};

TEST_F(FootpathRoutesTest, ConvertsTheRouteRoundACircleBetweenTwoNodes)
{
	auto holder = std::make_unique<route_planner::ObstacleGrid>();
	holder->AddObject(7, {200.0f, 100.0f}, 10.0f, 0);
	route_planner::RoutePlan plan;
	const route_planner::Point2D start {100.0f, 100.0f};
	const route_planner::Point2D dest {300.0f, 100.0f};
	plan.SetStart(start, 0.5f, *holder, -1, -1, 0);
	plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, (start.DistanceTo(dest) + 1.0f) * 5.0f, true);
	for (int i = 0; i < 128 && plan.GetState() == route_planner::RoutePlan::State::Searching; ++i)
	{
		plan.StepSearch(0);
	}
	ASSERT_EQ(plan.GetState(), route_planner::RoutePlan::State::Found);

	// a footpath with its two ends, as GenerateRoutesBetween makes it (the head first)
	const auto e = Reg().Create();
	auto& f = Reg().Assign<Footpath>(e);
	const auto headCoords = route_plan_world::ToMapCoords(start);
	const auto tailCoords = route_plan_world::ToMapCoords(dest);
	f.nodes.push_back({glm::vec3(0.0f), headCoords, 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), tailCoords, 1, f.nextNodeId++});
	footpaths::ConvertCreaturePlanToFootpath(*holder, plan, e, 0, 1, nullptr);

	const auto& nodes = Reg().Get<Footpath>(e).nodes;
	ASSERT_GT(nodes.size(), 4u); // the ends, the plan's start and dest, and the arc's steps
	EXPECT_EQ(nodes.front().id, 0);
	EXPECT_EQ(nodes.back().id, 1);
	// every new node stays out of the circle, has bits 0 and 1 and a new id
	for (size_t i = 1; i + 1 < nodes.size(); ++i)
	{
		const auto p = route_plan_world::ToPoint2D(nodes.at(i).coords);
		EXPECT_GE(p.DistanceTo({200.0f, 100.0f}), 10.0f - 0.01f);
		EXPECT_EQ(nodes.at(i).flags, 3);
		EXPECT_GE(nodes.at(i).id, 2);
	}
}

TEST_F(FootpathRoutesTest, KeepsTheHiddenNodesBetweenAfterTheNewOnes)
{
	auto holder = std::make_unique<route_planner::ObstacleGrid>();
	route_planner::RoutePlan plan;
	const route_planner::Point2D start {100.0f, 100.0f};
	const route_planner::Point2D dest {200.0f, 100.0f};
	plan.SetStart(start, 0.5f, *holder, -1, -1, 0);
	plan.SetDest(dest, 0.0f, 0.0f, 0.0f, -1, 0, 1000.0f, true);
	ASSERT_EQ(plan.GetState(), route_planner::RoutePlan::State::Found);

	const auto e = Reg().Create();
	auto& f = Reg().Assign<Footpath>(e);
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords(start), 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords({150.0f, 120.0f}), 1, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords({160.0f, 120.0f}), 4, f.nextNodeId++});
	f.nodes.push_back({glm::vec3(0.0f), route_plan_world::ToMapCoords(dest), 1, f.nextNodeId++});
	footpaths::ConvertCreaturePlanToFootpath(*holder, plan, e, 0, 3, nullptr);

	// EraseNodesBetween: node 1 deleted, node 2 (hidden) kept, chained to b after the new ones
	const auto& nodes = Reg().Get<Footpath>(e).nodes;
	ASSERT_EQ(nodes.size(), 5u); // a, the plan's start, its dest, the hidden one, b
	EXPECT_EQ(nodes.at(0).id, 0);
	EXPECT_EQ(nodes.at(3).id, 2);
	EXPECT_EQ(nodes.at(4).id, 3);
}

TEST_F(FootpathRoutesTest, AFinderMakesItsFootpathAtOnceAndGivesItToBothEndsWhenPlanned)
{
	const auto thing = [](float x, float z) {
		const auto e = Reg().Create();
		Reg().Assign<Transform>(e, glm::vec3(x, 0.0f, z), glm::mat3(1.0f), glm::vec3(1.0f));
		return e;
	};
	const auto pit = thing(100.0f, 100.0f);
	const auto site = thing(300.0f, 100.0f);
	footpaths::StartFootpathFinder(pit, site);
	// the footpath is there at once, with the site's arrive point and then the pit's at the head, both temporary
	ASSERT_EQ(Reg().Size<FootpathFinder>(), 1u);
	ASSERT_EQ(Reg().Size<Footpath>(), 1u);
	const auto footpath = footpaths::AtPositionFromHead(0);
	{
		const auto& nodes = Reg().Get<Footpath>(footpath).nodes;
		ASSERT_EQ(nodes.size(), 2u);
		EXPECT_EQ(route_plan_world::ToPoint2D(nodes.front().coords).x, 100.0f);
		EXPECT_EQ(route_plan_world::ToPoint2D(nodes.back().coords).x, 300.0f);
		EXPECT_EQ(nodes.front().flags, 1);
	}
	// not given to either end yet
	EXPECT_EQ(footpaths::GetFootpathLink(pit), entt::entity {entt::null});
	EXPECT_EQ(footpaths::GetFootpathLink(site), entt::entity {entt::null});
	// nothing in the way: the plan has its route, so the next turn gives the footpath to both ends and the finder goes
	footpaths::ProcessFootpathFindersTurn();
	EXPECT_EQ(Reg().Size<FootpathFinder>(), 0u);
	ASSERT_TRUE(Reg().Valid(footpath));
	for (const auto end : {pit, site})
	{
		const auto link = footpaths::GetFootpathLink(end);
		ASSERT_TRUE(link != entt::null);
		ASSERT_EQ(Reg().Get<FootpathLink>(link).footpaths.size(), 1u);
		EXPECT_EQ(Reg().Get<FootpathLink>(link).footpaths.front(), static_cast<Footpath::Id>(footpath));
	}
	// no node keeps the temporary bit
	for (const auto& node : Reg().Get<Footpath>(footpath).nodes)
	{
		EXPECT_EQ(node.flags & 2, 0);
	}
}

TEST_F(FootpathRoutesTest, AFinderWhoseEndWentIsDeletedWithItsFootpath)
{
	const auto pit = Reg().Create();
	Reg().Assign<Transform>(pit, glm::vec3(100.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	const auto site = Reg().Create();
	Reg().Assign<Transform>(site, glm::vec3(300.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	footpaths::StartFootpathFinder(pit, site);
	const auto footpath = footpaths::AtPositionFromHead(0);
	Reg().Destroy(site);
	footpaths::ProcessFootpathFindersTurn();
	EXPECT_EQ(Reg().Size<FootpathFinder>(), 0u);
	EXPECT_FALSE(Reg().Valid(footpath));
	EXPECT_EQ(footpaths::GetFootpathLink(pit), entt::entity {entt::null});
}

// A map unit past 2^24 is not a float: turned into one first and then scaled, it would round twice. Each point is the
// exact product rounded once.
TEST(RoutePlanWorldPoint, TakesEachMapUnitToMetresWithOneRounding)
{
	constexpr int32_t k_PastFloat = 16777217; // 2^24 + 1, 2560.000152587890625 m exactly
	const auto p = route_plan_world::ToPoint2D({k_PastFloat, -k_PastFloat, 7.0f});
	EXPECT_EQ(p.x, 2560.000244140625f); // rounded twice it would be 2560
	EXPECT_EQ(p.z, -2560.000244140625f);
	EXPECT_NE(p.x, static_cast<float>(k_PastFloat) * (10.0f / 65536.0f));
	// below 2^24 the two agree
	const auto q = route_plan_world::ToPoint2D({65536, 6553, 0.0f});
	EXPECT_EQ(q.x, 10.0f);
	EXPECT_EQ(q.z, 0.999908447265625f);
}
} // namespace
