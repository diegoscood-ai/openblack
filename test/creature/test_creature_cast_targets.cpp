/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a creature's food, wood and water casts may be cast at, on a registry of the test's own: food into a built
// storage pit or a built worship site, wood into any storage pit, by a building going up or by a workshop, water on a
// field whose food is nothing or on any tree, and what the mind's scans find for them

#define LOCATOR_IMPLEMENTATIONS

#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreaturePlanActions.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "creature/CreatureSystemWorld.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace mind_detail = openblack::ecs::systems::mind_detail;
using creature_plan_actions::Target;

namespace
{
class CreatureCastTargetsTest: public ::testing::Test
{
protected:
	/// Something standing on the land at a point
	static entt::entity Thing(glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	/// A storage pit, built or with half of it built
	static entt::entity Pit(bool built, glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = test::creature_world::World::Registry();
		const auto pit = Thing(position);
		registry.Assign<StoragePit>(pit);
		auto& abode = registry.Assign<Abode>(pit);
		if (!built)
		{
			abode.buildFlags = Abode::k_UnderConstruction;
			abode.percentBuilt = 0.5f;
		}
		return pit;
	}

	/// A worship site, built or not begun
	static entt::entity Site(bool built, glm::vec3 position = glm::vec3(0.0f))
	{
		auto& registry = test::creature_world::World::Registry();
		const auto site = Thing(position);
		registry.Assign<WorshipSite>(site);
		auto& build = registry.Assign<CitadelPartBuild>(site);
		if (!built)
		{
			build.buildFlags = CitadelPartBuild::k_UnderConstruction;
			build.percentBuilt = 0.0f;
		}
		return site;
	}

	/// A plain abode, whole or half built
	static entt::entity Hut(float percentBuilt = 1.0f)
	{
		const auto hut = Thing();
		test::creature_world::World::Registry().Assign<Abode>(hut).percentBuilt = percentBuilt;
		return hut;
	}

	void SetUp() override { _self = Thing(glm::vec3(0.0f, 0.0f, -50.0f)); }

	[[nodiscard]] bool Accepts(Target target, entt::entity entity) const
	{
		return mind_detail::Accepts(std::as_const(test::creature_world::World::Registry()), target, entity, _self);
	}

	/// The creature, as far as the tests ask: it is never its own target
	[[nodiscard]] entt::entity Self() const { return _self; }

private:
	test::creature_world::World _world;
	entt::entity _self {entt::null};
};
} // namespace

TEST_F(CreatureCastTargetsTest, FoodGoesIntoABuiltStoragePitOrABuiltWorshipSite)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing();
	registry.Assign<Villager>(villager);
	EXPECT_TRUE(Accepts(Target::FoodStore, Pit(true)));
	EXPECT_FALSE(Accepts(Target::FoodStore, Pit(false)));
	EXPECT_TRUE(Accepts(Target::FoodStore, Site(true)));
	EXPECT_FALSE(Accepts(Target::FoodStore, Site(false)));
	// a villager or a hut takes no food
	EXPECT_FALSE(Accepts(Target::FoodStore, villager));
	EXPECT_FALSE(Accepts(Target::FoodStore, Hut()));
}

TEST_F(CreatureCastTargetsTest, WoodGoesIntoAnyStoragePit)
{
	EXPECT_TRUE(Accepts(Target::WoodStore, Pit(true)));
	EXPECT_TRUE(Accepts(Target::WoodStore, Pit(false)));
	EXPECT_FALSE(Accepts(Target::WoodStore, Site(true)));
	EXPECT_FALSE(Accepts(Target::WoodStore, Hut()));
}

TEST_F(CreatureCastTargetsTest, AWorshipSiteBuiltOrNot)
{
	EXPECT_TRUE(Accepts(Target::WorshipSite, Site(true)));
	EXPECT_TRUE(Accepts(Target::WorshipSite, Site(false)));
	EXPECT_FALSE(Accepts(Target::WorshipSite, Pit(true)));
}

TEST_F(CreatureCastTargetsTest, ABuildingGoingUp)
{
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing();
	registry.Assign<Villager>(villager);
	EXPECT_TRUE(Accepts(Target::BuildingSite, Hut(0.5f)));
	EXPECT_TRUE(Accepts(Target::BuildingSite, Site(false)));
	EXPECT_FALSE(Accepts(Target::BuildingSite, Hut()));
	EXPECT_FALSE(Accepts(Target::BuildingSite, Pit(true)));
	EXPECT_FALSE(Accepts(Target::BuildingSite, villager));
}

TEST_F(CreatureCastTargetsTest, AWorkshop)
{
	const auto workshop = Hut();
	test::creature_world::World::Registry().Assign<Workshop>(workshop);
	EXPECT_TRUE(Accepts(Target::Workshop, workshop));
	EXPECT_FALSE(Accepts(Target::Workshop, Hut()));
}

TEST_F(CreatureCastTargetsTest, TheScansFindStoragePitsAndWorshipSitesNearestFirst)
{
	const auto pit = Pit(true, glm::vec3(10.0f, 0.0f, 0.0f));
	const auto site = Site(true, glm::vec3(5.0f, 0.0f, 0.0f));
	const auto unbuilt = Pit(false, glm::vec3(20.0f, 0.0f, 0.0f));
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	const auto food = mind_detail::Gather(registry, Target::FoodStore, Self(), glm::vec2(0.0f));
	ASSERT_EQ(food.size(), 2u);
	EXPECT_EQ(food[0].entity, site);
	EXPECT_EQ(food[1].entity, pit);
	const auto wood = mind_detail::Gather(registry, Target::WoodStore, Self(), glm::vec2(0.0f));
	ASSERT_EQ(wood.size(), 2u);
	EXPECT_EQ(wood[0].entity, pit);
	EXPECT_EQ(wood[1].entity, unbuilt);
}

TEST_F(CreatureCastTargetsTest, WaterIsSprinkledOnAFieldWhoseFoodIsNothingOrOnAnyTree)
{
	auto& registry = test::creature_world::World::Registry();
	const auto dry = Thing();
	registry.Assign<Field>(dry).town = -1;
	// ripe, with food in it
	const auto ripe = Thing();
	auto& field = registry.Assign<Field>(ripe);
	field.town = -1;
	field.growth = Field::k_AgeRecolt;
	field.food = 100.0f;
	const auto tree = Thing();
	registry.Assign<Tree>(tree);
	const auto villager = Thing();
	registry.Assign<Villager>(villager);

	EXPECT_TRUE(Accepts(Target::DryField, dry));
	EXPECT_FALSE(Accepts(Target::DryField, ripe));
	EXPECT_FALSE(Accepts(Target::DryField, tree));
	EXPECT_TRUE(Accepts(Target::Sprinklable, dry));
	EXPECT_FALSE(Accepts(Target::Sprinklable, ripe));
	EXPECT_TRUE(Accepts(Target::Sprinklable, tree));
	EXPECT_FALSE(Accepts(Target::Sprinklable, villager));
	EXPECT_TRUE(Accepts(Target::AnyTree, tree));
	EXPECT_FALSE(Accepts(Target::AnyTree, dry));
}

TEST_F(CreatureCastTargetsTest, TheScansFindDryFieldsAndTreesToWater)
{
	auto& registry = test::creature_world::World::Registry();
	const auto dry = Thing(glm::vec3(10.0f, 0.0f, 0.0f));
	registry.Assign<Field>(dry).town = -1;
	const auto tree = Thing(glm::vec3(5.0f, 0.0f, 0.0f));
	registry.Assign<Tree>(tree);
	const auto& view = std::as_const(registry);
	const auto sprinkled = mind_detail::Gather(view, Target::Sprinklable, Self(), glm::vec2(0.0f));
	ASSERT_EQ(sprinkled.size(), 2u);
	EXPECT_EQ(sprinkled[0].entity, tree);
	EXPECT_EQ(sprinkled[1].entity, dry);
	const auto fields = mind_detail::Gather(view, Target::DryField, Self(), glm::vec2(0.0f));
	ASSERT_EQ(fields.size(), 1u);
	EXPECT_EQ(fields[0].entity, dry);
	const auto trees = mind_detail::Gather(view, Target::AnyTree, Self(), glm::vec2(0.0f));
	ASSERT_EQ(trees.size(), 1u);
	EXPECT_EQ(trees[0].entity, tree);
}
