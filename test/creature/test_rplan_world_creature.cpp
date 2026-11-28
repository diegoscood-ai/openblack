/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The route planner's world for a creature: a footpath's holder (no plan) gets the circles it always got, a creature's
// follower is told apart by its plan, a creature in the walked cell is no obstacle to either, a map shield is avoided
// unless it is the creature's own player's, a way out of a land is kept clear of unless the creature may be taken and
// the other vortices go by the rule of any thing, a side step's end is judged against the circles of one square, and the
// follower's context and remaining length

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>
#include <cstdint>

#include <array>
#include <bit>
#include <memory>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "3D/MapCoords.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/LandscapeVortex.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/ScriptHeld.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/ScriptHeld.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "RoutePlanner/RouteFollower.h"
#include "creature/CreatureSystemWorld.h"
#include "support/CreatureFakes.h"
#include "support/FireFakes.h"
#include "support/MapFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using openblack::route_planner::ObstacleGrid;
using openblack::route_planner::RouteFollower;

namespace
{
class RoutePlanWorldCreatureTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::mapCellsSystem::emplace<systems::MapCellsSystem>();
		// a 32 x 32 mask of land, one avoided cell and one cell of water a creature can walk in, both next to (5, 5)
		auto& state = Locator::landAvoidSystem::value().GetState();
		state.size = 32;
		state.avoid.assign(32 * 32, land_avoid::k_Land);
		state.avoid[static_cast<size_t>(5 * 32 + 6)] = land_avoid::k_Avoid;
		state.avoid[static_cast<size_t>(4 * 32 + 5)] = land_avoid::k_Water;
	}

	test::creature_world::World _world;
	const test::RestoreService<Locator::mapCellsSystem> _restoreCells;
};
} // namespace

TEST_F(RoutePlanWorldCreatureTest, AFootpathHolderGetsTheCirclesItAlwaysGot)
{
	// no plan: the avoided cell and the water cell each give a circle, as the footpaths have always had them
	auto footpath = std::make_unique<ObstacleGrid>();
	ASSERT_EQ(footpath->GetPlan(), nullptr);
	route_plan_world::CheckSquareFunction(5, 5, *footpath);
	ASSERT_EQ(footpath->ObjectCount(), 2);
	EXPECT_EQ(footpath->Object(0).id, -1);
	// x outer, z inner: the water cell (5, 4) first, then the avoided cell (6, 5); each at its cell's centre
	EXPECT_FLOAT_EQ(footpath->Object(0).c.x, 55.0f);
	EXPECT_FLOAT_EQ(footpath->Object(0).c.z, 45.0f);
	EXPECT_FLOAT_EQ(footpath->Object(1).c.x, 65.0f);
	EXPECT_FLOAT_EQ(footpath->Object(1).c.z, 55.0f);

	// a creature's follower is its own plan: the water it can walk in is no obstacle. The creature is in the map cells
	// from its creation, in its own cell (10, 10), not in the walked one
	const auto creature = test::creature_world::World::MakeCreature();
	ASSERT_EQ(map_coords::CellOf(glm::vec3(100.0f, 0.0f, 100.0f)), glm::ivec2(10, 10));
	EXPECT_EQ(map_cells::MobileInCell(glm::ivec2(10, 10)), (std::vector<entt::entity> {creature}));
	EXPECT_TRUE(map_cells::MobileInCell(glm::ivec2(5, 5)).empty());
	auto follower = std::make_unique<RouteFollower>();
	follower->Init(static_cast<int32_t>(entt::to_integral(creature)), {}, {}, {}, 1);
	route_plan_world::CheckSquareFunction(5, 5, *follower);
	ASSERT_EQ(follower->ObjectCount(), 1);
	EXPECT_FLOAT_EQ(follower->Object(0).c.x, 65.0f);
}

TEST_F(RoutePlanWorldCreatureTest, ACreatureInTheWalkedCellIsNoObstacle)
{
	// the creature is filed in the cell when it is made, so the walk of that cell meets it; the creatures are never
	// obstacles (CreatureMustAvoid), for a footpath or for a creature's own plan, so nothing is added for it
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(55.0f, 0.0f, 55.0f));
	ASSERT_EQ(map_cells::MobileInCell(glm::ivec2(5, 5)), (std::vector<entt::entity> {creature}));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(creature));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(creature, creature));

	// the same circles as with the cell empty: the water and the avoided cell for a footpath, the avoided one for its
	// own follower
	auto footpath = std::make_unique<ObstacleGrid>();
	route_plan_world::CheckSquareFunction(5, 5, *footpath);
	ASSERT_EQ(footpath->ObjectCount(), 2);
	EXPECT_EQ(footpath->Object(0).id, -1);
	EXPECT_EQ(footpath->Object(1).id, -1);
	auto follower = std::make_unique<RouteFollower>();
	follower->Init(static_cast<int32_t>(entt::to_integral(creature)), {}, {}, {}, 1);
	route_plan_world::CheckSquareFunction(5, 5, *follower);
	ASSERT_EQ(follower->ObjectCount(), 1);
	EXPECT_EQ(follower->Object(0).id, -1);
}

// Another creature is walked round only while its route stands: never laid (no follower yet), stopped or being planned;
// not while it walks its route
TEST_F(RoutePlanWorldCreatureTest, AnotherCreatureIsWalkedRoundOnlyWhileItsRouteStands)
{
	ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	auto& registry = test::creature_world::World::Registry();
	const auto walker = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(55.0f, 0.0f, 55.0f));
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(other, walker));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(other));

	auto& follower = registry.Get<CreatureLocomotion>(other).follower;
	follower = std::make_unique<RouteFollower>();
	follower->Init(static_cast<int32_t>(entt::to_integral(other)), {}, {}, {}, 64);
	follower->SetIdle();
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(other, walker));

	const route_planner::Point2D start {55.0f, 55.0f};
	const route_planner::Point2D dest {255.0f, 55.0f};
	follower->SetPosition(start);
	follower->SetPlanStart(start);
	follower->SetDest(dest, 0.0f, 1.0f, 0.5f, (start.DistanceTo(dest) + 1.0f) * 5.0f);
	ASSERT_EQ(follower->GetState(), RouteFollower::State::Planning);
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(other, walker));
	for (int i = 0; i < 16 && follower->GetState() == RouteFollower::State::Planning; ++i)
	{
		follower->Update(nullptr, 0);
	}
	ASSERT_EQ(follower->GetState(), RouteFollower::State::Following);
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(other, walker));
}

TEST_F(RoutePlanWorldCreatureTest, ASideStepEndsOnlyOutsideTheCirclesOfTheSquareOfItsXAndItsHeight)
{
	ObstacleGrid::InstallCallbacks(&route_plan_world::CheckSquareFunction, nullptr);
	const auto creature = test::creature_world::World::MakeCreature();
	// at the height of the land, the square (0, 0): the avoided cell's circle refuses an end within it, while the water
	// the creature may walk in is no circle of its plan
	EXPECT_FALSE(route_plan_world::IsStepEndClear(creature, {.x = 65.0f, .z = 55.0f}, 0.0f));
	EXPECT_FALSE(route_plan_world::IsStepEndClear(creature, {.x = 60.0f, .z = 55.0f}, 0.0f));
	EXPECT_TRUE(route_plan_world::IsStepEndClear(creature, {.x = 55.0f, .z = 45.0f}, 0.0f));
	EXPECT_TRUE(route_plan_world::IsStepEndClear(creature, {.x = 75.0f, .z = 55.0f}, 0.0f));
	// the second square index is the end's height, not its z: 80 up, the square (0, 1) is read, with nothing avoided
	EXPECT_TRUE(route_plan_world::IsStepEndClear(creature, {.x = 65.0f, .z = 55.0f}, 80.0f));
	EXPECT_FALSE(route_plan_world::IsStepEndClear(creature, {.x = 65.0f, .z = 55.0f}, 79.0f));
	ObstacleGrid::InstallCallbacks(nullptr, nullptr);
}

TEST_F(RoutePlanWorldCreatureTest, AnotherPlayersShieldIsAvoided)
{
	auto& registry = test::creature_world::World::Registry();
	const auto shield = registry.Create();
	// its spell is gone: the shield takes the interface's player, the first
	registry.Assign<MagicShield>(shield);
	const auto mine = test::creature_world::World::MakeCreature(glm::vec3(100.0f), PlayerNames::PLAYER_ONE);
	const auto theirs = test::creature_world::World::MakeCreature(glm::vec3(100.0f), PlayerNames::PLAYER_TWO);

	// with no creature, as the footpaths ask, a shield is never in the way
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(shield));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(shield, mine));
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(shield, theirs));
	// another creature standing, its route not yet laid, is in the way of a creature, never of a footpath
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(theirs, mine));
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(theirs));
	// something gone is never in the way
	const auto gone = registry.Create();
	registry.Destroy(gone);
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(gone, mine));
}

TEST_F(RoutePlanWorldCreatureTest, AWayOutOfALandIsKeptClearOfUnlessTheCreatureMayBeTaken)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto creature = test::creature_world::World::MakeCreature();
	const auto wayOut = registry.Create();
	registry.Assign<LandscapeVortex>(wayOut).type = VortexType::In;
	// a footpath always goes round it, and so does a creature no script lets through
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(wayOut));
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(wayOut, creature));
	// let through by a script, but not on the leash: it still keeps clear
	registry.Get<Creature>(creature).mayGoThroughVortex = true;
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(wayOut, creature));
	// let through and on the leash, it may go near
	test::creature_loop_fakes::CallLog log;
	const test::RestoreService<Locator::leashSystem> restoreLeash;
	auto& leash = static_cast<test::creature_loop_fakes::FakeLeash&>(
	    Locator::leashSystem::emplace<test::creature_loop_fakes::FakeLeash>(log));
	leash.leashed = true;
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(wayOut, creature));
	// on the leash without being let through, it keeps clear
	registry.Get<Creature>(creature).mayGoThroughVortex = false;
	EXPECT_TRUE(route_plan_world::CreatureMustAvoid(wayOut, creature));
	// a script controlling it lets it go near, leashed or not
	leash.leashed = false;
	registry.Assign<ScriptHeld>(creature);
	script_held::SetControlledByScript(creature, true);
	EXPECT_FALSE(route_plan_world::CreatureMustAvoid(wayOut, creature));
}

TEST_F(RoutePlanWorldCreatureTest, TheOtherVorticesAreAvoidedByTheRuleOfAnyOtherThing)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the rule reads whether the thing burns, through the fire service
	const test::RestoreService<Locator::fireSystem> restoreFires;
	Locator::fireSystem::emplace<test::FakeFires>();
	// and the thing's height, through its mesh's box: a vortex is measured by the vortex cylinder, which has none here
	const test::RestoreService<Locator::meshBoxProvider> restoreBoxes;
	auto& boxes = static_cast<test::FakeMeshBoxProvider&>(Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>());
	boxes.meshBox = [](entt::id_type /*meshId*/) { return std::optional<AxisAlignedBoundingBox>(); };
	const auto creature = test::creature_world::World::MakeCreature();
	ASSERT_GT(object::GetHeight(creature), 0.0f);
	for (const auto type : {VortexType::Out, VortexType::Volcano})
	{
		const auto vortex = registry.Create();
		registry.Assign<LandscapeVortex>(vortex).type = type;
		// with no creature, always; with one, only as tall as a tenth of it or burning: this one has no height
		EXPECT_TRUE(route_plan_world::CreatureMustAvoid(vortex));
		ASSERT_EQ(object::GetHeight(vortex), 0.0f);
		EXPECT_FALSE(route_plan_world::CreatureMustAvoid(vortex, creature));
		// a script controlling the creature does not let it near one, as it does a way out
		registry.Assign<ScriptHeld>(creature);
		script_held::SetControlledByScript(creature, true);
		EXPECT_FALSE(route_plan_world::CreatureMustAvoid(vortex, creature));
		registry.Remove<ScriptHeld>(creature);
	}
}

TEST_F(RoutePlanWorldCreatureTest, AnObstacleEnteringTheMapIsToldToACreatureWhoseRouteIsUnderWay)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a burning tree is kept clear of by any creature; the fire service says it burns
	const test::RestoreService<Locator::fireSystem> restoreFires;
	auto& fires = static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
	const auto walking = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f));
	const auto idle = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 140.0f));
	ASSERT_TRUE(registry.AllOf<CreatureLocomotion>(walking));
	ASSERT_TRUE(registry.AllOf<CreatureLocomotion>(idle));
	const auto followerOf = [&registry](entt::entity creature) -> RouteFollower& {
		auto& follower = registry.Get<CreatureLocomotion>(creature).follower;
		follower = std::make_unique<RouteFollower>();
		follower->Init(static_cast<int32_t>(entt::to_integral(creature)), {}, {}, {}, 64);
		follower->SetIdle();
		return *follower;
	};
	// one creature's route is being planned, the other has none
	auto& planning = followerOf(walking);
	const route_planner::Point2D start {100.0f, 100.0f};
	planning.SetPosition(start);
	planning.SetPlanStart(start);
	planning.SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 1005.0f);
	ASSERT_EQ(planning.GetState(), RouteFollower::State::Planning);
	auto& resting = followerOf(idle);
	const auto planningBefore = planning.ObjectCount();
	const auto restingBefore = resting.ObjectCount();

	const auto tree = registry.Create();
	registry.Assign<Transform>(tree, glm::vec3(200.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<Tree>(tree);
	fires.Burn(tree, 1000.0f);
	ASSERT_TRUE(route_plan_world::CreatureMustAvoid(tree, walking));
	map_cells::InsertMapObject(tree);
	// the creature whose route is under way takes the tree into its plan; the one with no route does not
	EXPECT_EQ(planning.ObjectCount(), planningBefore + 1);
	EXPECT_EQ(resting.ObjectCount(), restingBefore);
}

TEST(RoutePlanWorldRules, EachKindOfThingIsAvoidedByItsClassRule)
{
	using route_plan_world::AvoidFacts;
	using route_plan_world::AvoidRule;
	using route_plan_world::MustAvoidByRule;
	// a creature 30 tall: a thing at least 3 tall is in its way, a tree at least 45 tall
	const AvoidFacts low {.height = 2.9f, .creatureHeight = 30.0f};
	const AvoidFacts tenth {.height = 3.0f, .creatureHeight = 30.0f};
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::TallOrBurning, low));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::TallOrBurning, tenth));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::TallOrBurning, {.height = 0.5f, .creatureHeight = 30.0f, .onFire = true}));
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::TreeTallOrBurning, {.height = 44.0f, .creatureHeight = 30.0f}));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::TreeTallOrBurning, {.height = 45.0f, .creatureHeight = 30.0f}));
	// a building just begun is not, however tall, unless it burns
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::BuildingTallOrBurning, {.height = 20.0f, .creatureHeight = 30.0f, .begun = false}));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::BuildingTallOrBurning, {.height = 20.0f, .creatureHeight = 30.0f}));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::BuildingTallOrBurning,
	                            {.height = 20.0f, .creatureHeight = 30.0f, .onFire = true, .begun = false}));
	// a dead tree, a field or a food pile only while it burns, and not for a creature a script controls
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::BurningUnlessScripted, {.height = 50.0f, .creatureHeight = 30.0f}));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::BurningUnlessScripted, {.onFire = true}));
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::BurningUnlessScripted, {.onFire = true, .creatureScripted = true}));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::WhileBurning, {.onFire = true}));
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::WhileBurning, tenth));
	EXPECT_TRUE(MustAvoidByRule(AvoidRule::Always, low));
	EXPECT_FALSE(MustAvoidByRule(AvoidRule::Never, {.height = 50.0f, .creatureHeight = 30.0f, .onFire = true}));
}

TEST(RouteFollowerCreature, ContextAndRemainingLength)
{
	ObstacleGrid::InstallCallbacks(nullptr, nullptr);
	auto follower = std::make_unique<RouteFollower>();
	follower->Init(42, {}, {}, {}, 64);
	follower->SetIdle();
	EXPECT_EQ(follower->GetContext(), 42);
	EXPECT_FLOAT_EQ(follower->RemainingLength(), 0.0f);

	const route_planner::Point2D start {100.0f, 100.0f};
	follower->SetPosition(start);
	follower->SetPlanStart(start);
	follower->SetDest({300.0f, 100.0f}, 0.0f, 1.0f, 0.5f, 1005.0f);
	for (int i = 0; i < 16 && follower->GetState() == RouteFollower::State::Planning; ++i)
	{
		follower->Update(nullptr, 0);
	}
	ASSERT_EQ(follower->GetState(), RouteFollower::State::Following);
	const auto whole = follower->RemainingLength();
	EXPECT_NEAR(whole, 200.0f, 1.0f);
	// 0.1 x 50 = 5 along it
	follower->SetSpeed(50.0f);
	follower->MoveAlongRoute();
	EXPECT_NEAR(follower->RemainingLength(), whole - 5.0f, 1e-3f);
}

TEST_F(RoutePlanWorldCreatureTest, TheHeartsArmsAreTheGamesSeventhOfATurnApart)
{
	// a heart unturned at (1000, 1000) with no temple's owner: its middle circle, two arms of six and five arms of
	// four. The first circle of the last arm is 15.91 m out at 3.83 plus six steps of 2 pi / 7 as the game's float
	auto& registry = test::creature_world::World::Registry();
	const auto heart = registry.Create();
	registry.Assign<Transform>(heart, glm::vec3(1000.0f, 0.0f, 1000.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<CitadelHeart>(heart);
	std::vector<std::pair<route_planner::Point2D, float>> circles;
	const auto collect = [](void* context, entt::entity, const route_planner::Point2D& centre, float radius, int32_t) {
		static_cast<std::vector<std::pair<route_planner::Point2D, float>>*>(context)->emplace_back(centre, radius);
	};
	auto footpath = std::make_unique<ObstacleGrid>();
	route_plan_world::AddToRoutePlan(heart, *footpath, 0, entt::null, collect, &circles);
	ASSERT_EQ(circles.size(), 33u);
	float theta = map_cells::detail::YAngleOf(glm::mat3(1.0f)) + 3.83f;
	for (int arm = 0; arm < 6; ++arm)
	{
		theta = theta + std::bit_cast<float>(0x3F65C8FAu);
	}
	const float dist = 21.5f * 0.1f + 21.5f * 0.64f;
	const auto c = static_cast<float>(std::cos(static_cast<double>(theta)));
	const auto s = static_cast<float>(std::sin(static_cast<double>(theta)));
	EXPECT_EQ(circles[29].first.x, dist * c + 1000.0f);
	EXPECT_EQ(circles[29].first.z, dist * s + 1000.0f);
}

// The steps between the circles along a long side, and the temple arm's turned circles, as the game's 24-bit FPU
// works them: the processor's own sine and cosine, each product rounded once, run through the oracle at the game's
// control word (20 of the 24 cases are ones a sine and cosine stored as floats first missed)
// (docs/wiki/beings/routes.md#rpholder-the-obstacles)
TEST(RoutePlanCircles, TheStepsAreTheFpusSineAndCosineRoundedOnce)
{
	struct Case
	{
		uint32_t angle;
		uint32_t length;
		uint32_t centre;
		std::array<uint32_t, 2> side;
		std::array<uint32_t, 2> turned;
	};
	constexpr std::array k_Cases = {
	    Case {0x40CFF44Fu, 0x41E418CEu, 0x441C429Fu, {0x40C3003Fu, 0xC1DED38Fu}, {0x4423393Bu, 0x441DC89Fu}},
	    Case {0x3F49A428u, 0x41534BD9u, 0x43ED0E35u, {0x4115BF2Bu, 0xC1151210u}, {0x43F1B6C6u, 0x43F1BC2Eu}},
	    Case {0xC04B337Fu, 0x420CE45Au, 0x43EB8976u, {0x3F96AC02u, 0x420CD034u}, {0x43D9EF70u, 0x43EC2022u}},
	    Case {0x40718B41u, 0x41FC83DEu, 0x4421C77Au, {0xC19548B7u, 0x41CBA970u}, {0x441B6A2Eu, 0x441D1D34u}},
	    Case {0xC04C822Cu, 0x41953687u, 0x456C3176u, {0x3F80816Bu, 0x4194FF27u}, {0x456B0778u, 0x456C4186u}},
	    Case {0x4021B040u, 0x3F12EBE3u, 0x446C4FC2u, {0x3EA995E9u, 0x3EEFF7A8u}, {0x446C31C3u, 0x446C64F5u}},
	    Case {0x40DA17EFu, 0x420B9C5Bu, 0x44998949u, {0x418DB1B6u, 0xC1F09919u}, {0x449D4BADu, 0x449BC010u}},
	    Case {0x40D30E7Eu, 0x41ACBC5Fu, 0x452B789Fu, {0x40D45033u, 0xC1A460F2u}, {0x452CC161u, 0x452BE2C7u}},
	    Case {0x3E95C1D1u, 0x40D5C21Bu, 0x44270F32u, {0x3FF68A9Eu, 0xC0CCADEEu}, {0x4428A88Eu, 0x44278A77u}},
	    Case {0xBE3C97B0u, 0x419A3289u, 0x4532035Bu, {0xC061E8B7u, 0xC19796F3u}, {0x45333289u, 0x4531CAE1u}},
	    Case {0x40B0DB77u, 0x3FC1B821u, 0x456CC9EEu, {0xBF84F348u, 0xBF8CE516u}, {0x456CDB8Bu, 0x456CB950u}},
	    Case {0x40C47A30u, 0x415A1A25u, 0x456785EEu, {0xBFF92016u, 0xC157DE17u}, {0x45685DCCu, 0x456766CAu}},
	    Case {0x400E5638u, 0x414884AEu, 0x44A6F0A3u, {0x411F3CFFu, 0x40F3BA35u}, {0x44A5FCE9u, 0x44A82F1Du}},
	    Case {0x40A5A2A2u, 0x419207D3u, 0x44DA1B4Fu, {0xC1829C01u, 0xC102A187u}, {0x44DB2092u, 0x44D810DFu}},
	    Case {0xBFF76A0Du, 0x42073040u, 0x449C38A7u, {0xC1FCD77Bu, 0x413F91EBu}, {0x449AB983u, 0x44984549u}},
	    Case {0x40851D1Bu, 0x41998081u, 0x449A5F9Fu, {0xC182A801u, 0x412124B2u}, {0x44991D56u, 0x449854FFu}},
	    Case {0x40DC1928u, 0x4219941Eu, 0x451F129Du, {0x41AC229Au, 0xC1FE6436u}, {0x45210F65u, 0x45206AE2u}},
	    Case {0xBF2407C6u, 0x41FA5551u, 0x4559ACF0u, {0xC195A5ACu, 0xC1C8AE16u}, {0x455B3E4Cu, 0x455881A5u}},
	    Case {0x4026343Eu, 0x3F033CDAu, 0x450B943Bu, {0x3E87FE8Du, 0x3EE07F38u}, {0x450B8D37u, 0x450B987Bu}},
	    Case {0xBF5E239Eu, 0x41D70E9Du, 0x44EE2EC9u, {0xC1A40F2Du, 0xC18B0C11u}, {0x44F05AF9u, 0x44EB9E8Cu}},
	    Case {0x40429706u, 0x41ED79A2u, 0x45481414u, {0x403FCA3Au, 0x41EC430Du}, {0x45463B8Eu, 0x45484407u}},
	    Case {0x40CCA9CEu, 0x41ECDF20u, 0x45671114u, {0x4054D0CDu, 0xC1EB5F82u}, {0x4568E7D3u, 0x45674648u}},
	    Case {0x40541C64u, 0x421029F5u, 0x44076035u, {0xC0C61EC9u, 0x420E0556u}, {0x43FCFFBFu, 0x4405D3F7u}},
	    Case {0x3FB66DC6u, 0x411E68CBu, 0x450ACAADu, {0x411CBBE0u, 0xBFB7D3F7u}, {0x450AE1A7u, 0x450B6769u}},
	};
	const auto f = [](uint32_t bits) { return std::bit_cast<float>(bits); };
	const auto bits = [](float value) { return std::bit_cast<uint32_t>(value); };
	for (const auto& c : k_Cases)
	{
		const auto side = ecs::route_plan_world::SideStep(f(c.angle), f(c.length));
		EXPECT_EQ(bits(side.x), c.side[0]);
		EXPECT_EQ(bits(side.z), c.side[1]);
		const auto turned = ecs::route_plan_world::TurnedArmPoint(f(c.angle), f(c.length), {f(c.centre), f(c.centre)});
		EXPECT_EQ(bits(turned.x), c.turned[0]);
		EXPECT_EQ(bits(turned.z), c.turned[1]);
	}
}
