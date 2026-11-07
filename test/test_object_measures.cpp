/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creatures' measures of things (ECS/Systems/Implementations/ObjectMeasures): the object size family's radius and
// height under raffclar's names, the route plan radius with a creature, which things a creature walks up to by its
// walk alone, and where a thing stands. Made-up things, no game data.

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureCastMoves.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBodyMetrics.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/RoutePlanWorld.h"
#include "ECS/Systems/Implementations/ObjectMeasures.h"
#include "Locator.h"
#include "RoutePlanner/ObstacleGrid.h"
#include "support/FireFakes.h"
#include "support/MapFakes.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace object = openblack::ecs::object;
namespace measures = openblack::ecs::systems::object_measures;

namespace
{
constexpr entt::id_type k_Box = 1;  ///< min (-1, 0, -2), max (1, 4, 2): half extents (1, 2, 2)
constexpr entt::id_type k_Tall = 2; ///< min (-3, 0, -1), max (3, 40, 1): half extents (3, 20, 1)

std::optional<AxisAlignedBoundingBox> Boxes(entt::id_type id)
{
	switch (id)
	{
	case k_Box:
		return AxisAlignedBoundingBox {{-1.0f, 0.0f, -2.0f}, {1.0f, 4.0f, 2.0f}};
	case k_Tall:
		return AxisAlignedBoundingBox {{-3.0f, 0.0f, -1.0f}, {3.0f, 40.0f, 1.0f}};
	default:
		return std::nullopt;
	}
}

class ObjectMeasures: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::EmplaceMapAndVillagerDefaults();
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		test::FakeMeshBoxProvider meshBoxes;
		meshBoxes.meshBox = &Boxes;
		Locator::meshBoxProvider::emplace<test::FakeMeshBoxProvider>(meshBoxes);
	}
	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
		test::ResetMapAndVillagerDefaults();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }

	static entt::entity Make(entt::id_type mesh, float scale, glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = Reg();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(scale));
		registry.Assign<Mesh>(entity, mesh, static_cast<int8_t>(0), static_cast<int8_t>(0));
		return entity;
	}

	/// A creature of size 1, its body's rest height 48 and reach 16: radius 5, height 15
	static entt::entity MakeCreature()
	{
		const auto creature = Make(k_Box, 2.0f);
		Reg().Assign<Creature>(creature, Creature {.size = 1.0f});
		Reg().Assign<CreatureBodyMetrics>(creature, CreatureBodyMetrics {.restHeight = 48.0f, .reach = 16.0f});
		return creature;
	}
};
} // namespace

TEST_F(ObjectMeasures, RadiusAndHeightAreTheObjectSizeFamilys)
{
	auto& registry = Reg();
	const auto box = Make(k_Box, 1.5f);
	EXPECT_EQ(measures::TwoDRadius(registry, box), 3.0f); // 1.5 x max(1, 2)
	EXPECT_EQ(measures::Height(registry, box), 6.0f);     // 2 x (2 x 1.5)
	const auto field = Make(k_Tall, 1.0f);
	registry.Assign<Field>(field);
	EXPECT_EQ(measures::TwoDRadius(registry, field), object::k_FieldRadius);
	EXPECT_EQ(measures::Height(registry, field), object::GetHeight(field));
	const auto creature = MakeCreature();
	EXPECT_EQ(measures::TwoDRadius(registry, creature), 5.0f);
	EXPECT_EQ(measures::Height(registry, creature), 15.0f);
	// none for a thing without a model, or gone
	const auto bare = registry.Create();
	EXPECT_EQ(measures::TwoDRadius(registry, bare), 0.0f);
	EXPECT_EQ(measures::Height(registry, bare), 0.0f);
	registry.Destroy(bare);
	EXPECT_EQ(measures::TwoDRadius(registry, bare), 0.0f);
}

TEST_F(ObjectMeasures, RoutePlanRadiusWithACreature)
{
	auto& registry = Reg();
	const auto creature = MakeCreature();
	// a low thing (height 6 of the creature's 15): its radius less (0.7 - 6 / 12 x 0.3) of the creature's nav radius,
	// its radius (5) at its own size out of the pen
	const auto box = Make(k_Box, 1.5f);
	EXPECT_EQ(measures::RoutePlanRadius(registry, box, creature),
	          creature_cast_moves::RoutePlanRadius(3.0f, 6.0f, false, 15.0f, 5.0f));
	EXPECT_EQ(measures::RoutePlanRadius(registry, box, creature), 3.0f - 5.0f * (0.7f - (6.0f / 12.0f) * 0.3f));
	EXPECT_FLOAT_EQ(measures::RoutePlanRadius(registry, box, creature), 3.0f - (0.7f - 0.3f * 6.0f / 12.0f) * 5.0f);
	// with no creature, the thing's own
	EXPECT_EQ(measures::RoutePlanRadius(registry, box, entt::null), object::GetRoutePlanRadius(box));
	// a tall one (80): less two fifths of it
	const auto tall = Make(k_Tall, 2.0f);
	EXPECT_FLOAT_EQ(measures::RoutePlanRadius(registry, tall, creature), 6.0f - 0.4f * 5.0f);
	// a tree's and the temple's are their own, whatever the creature
	const auto tree = Make(k_Tall, 2.0f);
	registry.Assign<Tree>(tree);
	EXPECT_EQ(measures::RoutePlanRadius(registry, tree, creature), object::GetRoutePlanRadius(tree));
	EXPECT_EQ(measures::RoutePlanRadius(registry, tree, creature), 0.25f);
	const auto temple = Make(k_Box, 1.5f);
	registry.Assign<Temple>(temple, PlayerNames::PLAYER_ONE);
	EXPECT_EQ(measures::RoutePlanRadius(registry, temple, creature), 3.0f * 0.33f);
	// in its temple's pen it is drawn smaller, but its nav radius is still that of its own size
	registry.Assign<CreatureDrawPose>(creature, CreatureDrawPose {.size = 0.5f});
	EXPECT_EQ(measures::RoutePlanRadius(registry, box, creature),
	          creature_cast_moves::RoutePlanRadius(3.0f, 6.0f, false, 7.5f, (1.0f / 0.5f) * 2.5f));
}

TEST_F(ObjectMeasures, ACreaturesRoutePlanKeepsThingsOffByItsShareAndMoreForAFire)
{
	// the circles a rock (radius 3, height 6), a tree (radius 2: 0.2) and a rock with no model put into a plan, its
	// margin the creature's nav radius (5)
	auto& registry = Reg();
	const auto creature = MakeCreature();
	const auto rock = Make(k_Box, 1.5f);
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Boulder1Chalk);
	const auto tree = Make(k_Box, 1.0f);
	registry.Assign<Tree>(tree);
	const auto bare = registry.Create();
	registry.Assign<Transform>(bare, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MobileStatic>(bare, MobileStaticInfo::Boulder1Chalk);
	auto holder = std::make_unique<route_planner::ObstacleGrid>();
	holder->SetMargin(5.0f);
	const auto circle = [&holder](entt::entity thing, entt::entity planner) {
		std::vector<float> radii;
		ecs::route_plan_world::AddToRoutePlan(
		    thing, *holder, 0, planner,
		    [](void* context, entt::entity, const route_planner::Point2D&, float radius, int32_t) {
			    static_cast<std::vector<float>*>(context)->push_back(radius);
		    },
		    &radii);
		EXPECT_EQ(radii.size(), 1u);
		return radii.empty() ? 0.0f : radii.front();
	};
	// with no creature, as the footpaths ask: the radius and the margin
	EXPECT_EQ(circle(rock, entt::null), 3.0f + 5.0f);
	EXPECT_EQ(circle(tree, entt::null), 0.2f + 5.0f);
	// a creature's plan: the rock less the creature's share, the tree as it is
	EXPECT_EQ(circle(rock, creature), creature_cast_moves::RoutePlanRadius(3.0f, 6.0f, false, 15.0f, 5.0f) + 5.0f);
	EXPECT_EQ(circle(tree, creature), 0.2f + 5.0f);
	// at least 0.05: with no margin, no model is 0 less seven tenths of the creature's 5
	holder->SetMargin(0.0f);
	EXPECT_EQ(circle(bare, creature), 0.05f);
	holder->SetMargin(5.0f);
	// a fire on a thing keeps a creature's plan 5 further off; not a footpath's
	auto& fires = static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
	fires.Burn(rock, 0.0f);
	fires.Burn(tree, 0.0f);
	EXPECT_EQ(circle(rock, creature), (creature_cast_moves::RoutePlanRadius(3.0f, 6.0f, false, 15.0f, 5.0f) + 5.0f) + 5.0f);
	EXPECT_EQ(circle(tree, creature), (0.2f + 5.0f) + 5.0f);
	EXPECT_EQ(circle(rock, entt::null), 3.0f + 5.0f);
}

TEST_F(ObjectMeasures, WalkedUpToItself)
{
	auto& registry = Reg();
	const auto abode = Make(k_Box, 1.0f);
	registry.Assign<Abode>(abode, AbodeNumber::A, Abode::k_NoTown, 0u, 0u);
	const auto feature = Make(k_Box, 1.0f);
	registry.Assign<Feature>(feature);
	const auto field = Make(k_Box, 1.0f);
	registry.Assign<Field>(field);
	const auto rock = Make(k_Box, 1.0f);
	registry.Assign<MobileStatic>(rock, MobileStaticInfo::Boulder1Chalk);
	const auto tree = Make(k_Box, 1.0f);
	registry.Assign<Tree>(tree);
	for (const auto thing : {abode, feature, field, rock, tree})
	{
		EXPECT_TRUE(measures::WalkedUpToItself(registry, thing));
	}
	EXPECT_FALSE(measures::WalkedUpToItself(registry, Make(k_Box, 1.0f)));
	EXPECT_FALSE(measures::WalkedUpToItself(registry, MakeCreature()));
}

TEST_F(ObjectMeasures, PositionOf)
{
	auto& registry = Reg();
	const auto thing = Make(k_Box, 1.0f, {10.0f, 2.0f, -30.0f});
	EXPECT_EQ(measures::PositionOf(registry, thing), glm::vec3(10.0f, 2.0f, -30.0f));
	const auto bare = registry.Create();
	EXPECT_FALSE(measures::PositionOf(registry, bare).has_value());
	registry.Destroy(thing);
	EXPECT_FALSE(measures::PositionOf(registry, thing).has_value());
}
