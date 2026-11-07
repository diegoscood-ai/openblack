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
// field whose food is nothing or on any tree, and what the mind's scans find for them. Also what getting high may be
// done to: only a mushroom

#define LOCATOR_IMPLEMENTATIONS

#include <utility>
#include <vector>

#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreaturePlanActions.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/StoragePit.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
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

	/// The game's tables, which the test fills in as it needs
	[[nodiscard]] InfoConstants& Info() { return _world.Info(); }

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

TEST_F(CreatureCastTargetsTest, ATownsShieldGoesOnlyToABuiltStoragePit)
{
	EXPECT_TRUE(Accepts(Target::BuiltStoragePit, Pit(true)));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, Pit(false)));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, Site(true)));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, Hut()));
}

TEST_F(CreatureCastTargetsTest, AThingNotOnFireIsNoFireToPutOut)
{
	// What burns is asked of the fires; a hut, a villager or a pit that was never set alight is no fire to put out
	auto& registry = test::creature_world::World::Registry();
	const auto villager = Thing();
	registry.Assign<Villager>(villager);
	EXPECT_FALSE(Accepts(Target::Burning, Hut()));
	EXPECT_FALSE(Accepts(Target::Burning, villager));
	EXPECT_FALSE(Accepts(Target::Burning, Pit(true)));
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

TEST_F(CreatureCastTargetsTest, AHandfulIsTakenFromAFieldWithFoodAndGivenToABuiltStoragePit)
{
	auto& registry = test::creature_world::World::Registry();
	const auto dry = Thing(glm::vec3(10.0f, 0.0f, 0.0f));
	registry.Assign<Field>(dry).town = -1;
	// grown some, with food in it
	const auto ripe = Thing(glm::vec3(5.0f, 0.0f, 0.0f));
	auto& field = registry.Assign<Field>(ripe);
	field.town = -1;
	field.growth = 1.0f;
	field.food = 100.0f;
	const auto pit = Pit(true, glm::vec3(20.0f, 0.0f, 0.0f));
	const auto unbuilt = Pit(false, glm::vec3(15.0f, 0.0f, 0.0f));

	EXPECT_TRUE(Accepts(Target::FieldWithFood, ripe));
	EXPECT_FALSE(Accepts(Target::FieldWithFood, dry));
	EXPECT_FALSE(Accepts(Target::FieldWithFood, pit));
	EXPECT_TRUE(Accepts(Target::BuiltStoragePit, pit));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, unbuilt));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, Site(true)));
	EXPECT_FALSE(Accepts(Target::BuiltStoragePit, Hut()));

	const auto& view = std::as_const(registry);
	const auto fields = mind_detail::Gather(view, Target::FieldWithFood, Self(), glm::vec2(0.0f));
	ASSERT_EQ(fields.size(), 1u);
	EXPECT_EQ(fields[0].entity, ripe);
	const auto pits = mind_detail::Gather(view, Target::BuiltStoragePit, Self(), glm::vec2(0.0f));
	ASSERT_EQ(pits.size(), 1u);
	EXPECT_EQ(pits[0].entity, pit);
}

TEST_F(CreatureCastTargetsTest, GettingHighIsOnlyDoneToAMushroom)
{
	auto& registry = test::creature_world::World::Registry();
	// the magic mushroom that grows on the land, and a feature of another kind
	const auto grown = Thing();
	registry.Assign<Feature>(grown).type = FeatureInfo::MagicMushroom;
	const auto otherFeature = Thing();
	registry.Assign<Feature>(otherFeature).type = FeatureInfo::Toadstool;
	// what lies about: a toadstool, a magic mushroom, and a lump of poo, by the class of their rows
	Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::Toadstool)).mobileType = MobileObjectInfo::Toadstool;
	Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::MagicMushroom)).mobileType = MobileObjectInfo::MagicMushroom;
	Info().mobileObject.at(static_cast<size_t>(MobileObjectInfo::LumpOfPoo)).mobileType = MobileObjectInfo::LumpOfPoo;
	const auto toadstool = Thing();
	registry.Assign<MobileObject>(toadstool).type = MobileObjectInfo::Toadstool;
	const auto lying = Thing();
	registry.Assign<MobileObject>(lying).type = MobileObjectInfo::MagicMushroom;
	const auto poo = Thing();
	registry.Assign<MobileObject>(poo).type = MobileObjectInfo::LumpOfPoo;
	// a villager, the football and a static
	const auto villager = Thing();
	registry.Assign<Villager>(villager);
	const auto ball = Thing();
	registry.Assign<MobileObject>(ball).type = MobileObjectInfo::Ball;
	registry.Assign<Ball>(ball);
	const auto still = Thing();
	registry.Assign<MobileStatic>(still);

	EXPECT_TRUE(Accepts(Target::Mushroom, grown));
	EXPECT_TRUE(Accepts(Target::Mushroom, toadstool));
	EXPECT_TRUE(Accepts(Target::Mushroom, lying));
	EXPECT_FALSE(Accepts(Target::Mushroom, otherFeature));
	EXPECT_FALSE(Accepts(Target::Mushroom, poo));
	EXPECT_FALSE(Accepts(Target::Mushroom, villager));
	EXPECT_FALSE(Accepts(Target::Mushroom, ball));
	EXPECT_FALSE(Accepts(Target::Mushroom, still));

	// the scan finds the mushrooms, the one grown on the land too, and nothing else
	const auto found = mind_detail::Gather(std::as_const(registry), Target::Mushroom, Self(), glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 3u);
	for (const auto& thing : found)
	{
		EXPECT_TRUE(thing.entity == grown || thing.entity == toadstool || thing.entity == lying);
	}
}

TEST_F(CreatureCastTargetsTest, ACreatureEatsFromAFieldWithFoodAPitWithFoodInItsPileOrABigEnoughTree)
{
	auto& registry = test::creature_world::World::Registry();
	// A field grown some with food in it; one with no growth; one grown with no food
	const auto ripe = Thing(glm::vec3(5.0f, 0.0f, 0.0f));
	auto& field = registry.Assign<Field>(ripe);
	field.town = -1;
	field.growth = 1.0f;
	field.food = 100.0f;
	const auto unsown = Thing(glm::vec3(6.0f, 0.0f, 0.0f));
	auto& bare = registry.Assign<Field>(unsown);
	bare.town = -1;
	bare.food = 100.0f;
	const auto eaten = Thing(glm::vec3(7.0f, 0.0f, 0.0f));
	auto& empty = registry.Assign<Field>(eaten);
	empty.town = -1;
	empty.growth = 1.0f;
	EXPECT_TRUE(Accepts(Target::FieldWithFood, ripe));
	EXPECT_FALSE(Accepts(Target::FieldWithFood, unsown));
	EXPECT_FALSE(Accepts(Target::FieldWithFood, eaten));
	EXPECT_FALSE(Accepts(Target::FieldWithFood, Pit(true)));

	// A pit and a tree are weighed against the drawn size of the one choosing, which is a creature
	registry.Assign<Creature>(Self());
	const auto pile = Thing(glm::vec3(20.0f, 0.0f, 0.0f));
	registry.Assign<Pot>(pile, Pot {.amount = 10, .maxAmount = 100, .type = PotInfo::FoodPile});
	const auto pit = Pit(true, glm::vec3(20.0f, 0.0f, 0.0f));
	registry.Get<StoragePit>(pit).foodPile = pile;
	const auto noPile = Pit(true, glm::vec3(25.0f, 0.0f, 0.0f));
	registry.Get<StoragePit>(noPile).foodPile = entt::null;
	const auto tree = Thing(glm::vec3(30.0f, 0.0f, 0.0f));
	registry.Assign<Tree>(tree);

	EXPECT_TRUE(Accepts(Target::StoragePitWithFood, pit));
	EXPECT_FALSE(Accepts(Target::StoragePitWithFood, noPile));
	EXPECT_FALSE(Accepts(Target::StoragePitWithFood, ripe));
	// A tree with no model has no height, so it is never tall enough
	EXPECT_FALSE(Accepts(Target::TreeBigEnough, tree));
	EXPECT_FALSE(Accepts(Target::TreeBigEnough, ripe));

	const auto& view = std::as_const(registry);
	const auto fields = mind_detail::Gather(view, Target::FieldWithFood, Self(), glm::vec2(0.0f));
	ASSERT_EQ(fields.size(), 1u);
	EXPECT_EQ(fields[0].entity, ripe);
	const auto pits = mind_detail::Gather(view, Target::StoragePitWithFood, Self(), glm::vec2(0.0f));
	ASSERT_EQ(pits.size(), 1u);
	EXPECT_EQ(pits[0].entity, pit);

	// A pile with no food, or one on its way out, has none for the creature
	registry.Get<Pot>(pile).amount = 0;
	EXPECT_FALSE(Accepts(Target::StoragePitWithFood, pit));
	registry.Get<Pot>(pile).amount = 10;
	registry.Assign<Unavailable>(pile);
	EXPECT_FALSE(Accepts(Target::StoragePitWithFood, pit));
}

TEST_F(CreatureCastTargetsTest, ATemplePartIsATempleAWorshipSiteOrTheAltarAtItsHead)
{
	auto& registry = test::creature_world::World::Registry();
	const auto temple = Thing(glm::vec3(30.0f, 0.0f, 0.0f));
	registry.Assign<Temple>(temple).owner = PlayerNames::PLAYER_ONE;
	const auto site = Site(true, glm::vec3(20.0f, 0.0f, 0.0f));
	const auto altar = Thing(glm::vec3(10.0f, 0.0f, 0.0f));
	registry.Assign<WorshipTotem>(altar, site);
	EXPECT_TRUE(Accepts(Target::TemplePart, temple));
	EXPECT_TRUE(Accepts(Target::TemplePart, site));
	EXPECT_TRUE(Accepts(Target::TemplePart, altar));
	EXPECT_FALSE(Accepts(Target::TemplePart, Pit(true)));
	// the altar is a part of its own, apart from its site: the scan finds all three, nearest first
	const auto parts = mind_detail::Gather(std::as_const(registry), Target::TemplePart, Self(), glm::vec2(0.0f));
	ASSERT_EQ(parts.size(), 3u);
	EXPECT_EQ(parts[0].entity, altar);
	EXPECT_EQ(parts[1].entity, site);
	EXPECT_EQ(parts[2].entity, temple);
}
