/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A creature among the physics objects: the lower speed at which what a creature lets go of is thrown, the heavy body
// it stands as for what is thrown at it, its weight, and what it lets go of going into the physics

#define LOCATOR_IMPLEMENTATIONS

#include <memory>
#include <optional>
#include <stdexcept>
#include <utility>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/CreaturePhysics.h"
#include "ECS/CreaturePose.h"
#include "ECS/MapCells.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/PhysicsGameHooks.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "ECS/Systems/Implementations/DynamicsSystem.h"
#include "ECS/Systems/Implementations/MapCellsSystem.h"
#include "Hand/HandGrabRules.h"
#include "Locator.h"
#include "Physics/Body.h"
#include "Physics/LivingRules.h"
#include "creature/CreatureSystemWorld.h"
#include "support/DynamicsFakes.h"
#include "support/RestoreService.h"
#include "support/TestServices.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::creature_physics::Release;
using openblack::ecs::systems::CreatureObjectActionSystem;
using openblack::hand_grab::IsThrow;
using openblack::physics::Body;

namespace
{
TEST(CreaturePhysicsRules, ACreatureThrowsAtALowerSpeedThanTheHand)
{
	// the speed across the ground squared: 1, 2.25 and 4
	EXPECT_FALSE(IsThrow(glm::vec3(1.0f, 0.0f, 0.0f), true));
	EXPECT_FALSE(IsThrow(glm::vec3(1.0f, 0.0f, 0.0f), false));
	EXPECT_TRUE(IsThrow(glm::vec3(1.5f, 0.0f, 0.0f), true));
	EXPECT_FALSE(IsThrow(glm::vec3(1.5f, 0.0f, 0.0f), false));
	EXPECT_TRUE(IsThrow(glm::vec3(0.0f, 0.0f, 2.0f), true));
	EXPECT_FALSE(IsThrow(glm::vec3(0.0f, 0.0f, 2.0f), false));
	EXPECT_TRUE(IsThrow(glm::vec3(2.0f, 0.0f, 0.5f), false));
	// up and down is not across the ground
	EXPECT_FALSE(IsThrow(glm::vec3(0.0f, 9.0f, 0.0f), true));
}

TEST(CreaturePhysicsRules, ACreaturesBodyIsAHeavyBallAsTallAsItThatFacesOut)
{
	const auto shape = ecs::creature_physics::ShapeOf(15.0f);
	EXPECT_FLOAT_EQ(shape.radius, 7.5f);
	EXPECT_EQ(shape.centre, glm::vec3(0.0f, 7.5f, 0.0f));
	EXPECT_FLOAT_EQ(shape.mass, 1000.0f);
	EXPECT_FALSE(shape.dynamic);
	ASSERT_EQ(shape.points.size(), 6u);
	ASSERT_EQ(shape.faces.size(), 8u);
	for (const auto& point : shape.points)
	{
		EXPECT_FLOAT_EQ(glm::length(point), 7.5f);
	}
	for (const auto& face : shape.faces)
	{
		const auto& a = shape.points.at(face[0]);
		const auto& b = shape.points.at(face[1]);
		const auto& c = shape.points.at(face[2]);
		EXPECT_GT(glm::dot(glm::cross(b - a, c - a), a + b + c), 0.0f);
	}
}

TEST(CreaturePhysicsRules, OnlyACreatureCanThrowAtATarget)
{
	const auto creature = static_cast<entt::entity>(5);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(true, creature), Release::AtTarget);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(false, creature), Release::LetGo);
	EXPECT_EQ(ecs::creature_physics::ReleaseOf(true, entt::null), Release::LetGo);
}

class CreaturePhysicsTest: public ::testing::Test
{
protected:
	void SetUp() override { test::EmplaceMapAndVillagerDefaults(); }
	void TearDown() override { test::ResetMapAndVillagerDefaults(); }

	// the world lists a thing put down reaches (the map cells, the fires, the animals' shared state...), reset after
	// the registry as the game's shutdown does
	const test::ScopedWorldSystems _worldSystems;
	test::creature_world::World _world;
};

TEST_F(CreaturePhysicsTest, ACreatureStandsAsItsBodyWhereItIs)
{
	const glm::vec3 at(100.0f, 2.0f, 50.0f);
	const auto creature = test::creature_world::World::MakeCreature(at);
	Body body;
	ASSERT_TRUE(ecs::creature_physics::SetUpBody(creature, body));
	EXPECT_FLOAT_EQ(body.Mass(), 1000.0f);
	EXPECT_FLOAT_EQ(body.Radius(), 7.5f);
	EXPECT_NEAR(glm::distance(body.Centre(), at + glm::vec3(0.0f, 7.5f, 0.0f)), 0.0f, 1e-4f);
	EXPECT_EQ(body.Points().size(), 6u);

	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, at, glm::mat3(1.0f), glm::vec3(1.0f));
	Body none;
	EXPECT_FALSE(ecs::creature_physics::SetUpBody(rock, none));
}

TEST_F(CreaturePhysicsTest, OnceRegisteredThePhysicsBuildsAndWeighsACreatureAsOne)
{
	const auto creature = test::creature_world::World::MakeCreature();
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(test::creature_world::World::Registry()));
	dynamics.SetClassHooks(std::make_unique<ecs::PhysicsGameHooks>());
	// the physics weighs it as its body, whatever it weighs as an object
	EXPECT_FLOAT_EQ(dynamics.WeightOf(creature), 1000.0f);
	Body body;
	ASSERT_TRUE(dynamics.SetUpBody(creature, body, false));
	EXPECT_FLOAT_EQ(body.Mass(), 1000.0f);
	// it never flies itself
	EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(creature));
}

// A script's scroll is nothing to the physics or a creature, as the original's class says: it never flies, stops nothing
// that flies, and a creature never picks it up or stones it
TEST_F(CreaturePhysicsTest, AScriptsHighlightIsNothingToThePhysicsOrACreature)
{
	auto& registry = test::creature_world::World::Registry();
	const auto highlight = registry.Create();
	registry.Assign<ecs::components::Transform>(highlight);
	registry.Assign<ecs::components::ScriptHighlight>(highlight);
	ecs::systems::DynamicsSystem dynamics(std::make_unique<test::FakeDynamicsWorld>(registry));
	dynamics.SetClassHooks(std::make_unique<ecs::PhysicsGameHooks>());
	EXPECT_FALSE(dynamics.CanBecomeAPhysicsObject(highlight));
	EXPECT_FALSE(dynamics.InteractsWithPhysicsObjects(highlight));
	const auto facts = ecs::systems::CreatureObjectActionSystem::KindFactsOf(highlight);
	EXPECT_EQ(facts.thing, creature_object_actions::Thing::Other);
	EXPECT_EQ(facts.rule, creature_object_actions::KindRule::Never);
	EXPECT_FALSE(facts.holdable);
	EXPECT_FALSE(creature_object_actions::CanBeStoned(facts.thing, false));
}

TEST_F(CreaturePhysicsTest, ACreatureWeighsItsMassByTheSizeItIsDrawnAtAndTheShapeItIsToBecome)
{
	auto& registry = test::creature_world::World::Registry();
	const auto creature =
	    test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_ONE, 0.0f, 1.0f);
	const auto target = ecs::creature_pose::TargetMorph(std::as_const(registry), creature);
	EXPECT_EQ(ecs::creature_physics::Weight(std::as_const(registry), creature),
	          openblack::physics::living::CreatureMass(1.0f, target.thinFat, target.weakStrong));
	// far more than the 1000 its body weighs in the physics
	EXPECT_GT(ecs::creature_physics::Weight(std::as_const(registry), creature), 1000.0f);

	// drawn smaller than its own size, as in its pen, it weighs what it is drawn at
	registry.Get<CreatureDrawPose>(creature).size = 0.5f;
	EXPECT_EQ(ecs::creature_physics::Weight(std::as_const(registry), creature),
	          openblack::physics::living::CreatureMass(0.5f, target.thinFat, target.weakStrong));

	// fatter and stronger than its body shows yet, it weighs what it is to become
	auto& body = registry.Get<Creature>(creature);
	body.fatness = 1.0f;
	body.strength = 1.0f;
	registry.Get<CreatureMorph>(creature).shownFatness = 1.0f;
	const auto heavier = ecs::creature_pose::TargetMorph(std::as_const(registry), creature);
	EXPECT_GT(heavier.thinFat, target.thinFat);
	EXPECT_EQ(ecs::creature_physics::Weight(std::as_const(registry), creature),
	          openblack::physics::living::CreatureMass(0.5f, heavier.thinFat, heavier.weakStrong));
}

TEST_F(CreaturePhysicsTest, WeighingSomethingThatIsNotACreatureFailsClearly)
{
	auto& registry = test::creature_world::World::Registry();
	const auto thing = registry.Create();

	EXPECT_THROW((void)ecs::creature_physics::Weight(std::as_const(registry), thing), std::invalid_argument);
}

/// The class hooks of a thing whose body needs no model: the creature class's own body, an empty one
class EmptyBodyHooks final: public ecs::systems::PhysicsClassHooks
{
public:
	bool SetUpBody([[maybe_unused]] ecs::systems::DynamicsSystemInterface& dynamics, [[maybe_unused]] entt::entity object,
	               [[maybe_unused]] Body& body) override
	{
		return true;
	}
};

TEST_F(CreaturePhysicsTest, WhatACreatureThrowsAtATargetIsItsPlayersThrowWithTheCreatureAsItsThrower)
{
	auto& registry = test::creature_world::World::Registry();
	const test::RestoreService<Locator::dynamicsSystem> restoreDynamics;
	auto& dynamics =
	    Locator::dynamicsSystem::emplace<ecs::systems::DynamicsSystem>(std::make_unique<test::FakeDynamicsWorld>(registry));
	dynamics.SetClassHooks(std::make_unique<EmptyBodyHooks>());
	const auto creature = test::creature_world::World::MakeCreature(glm::vec3(100.0f, 0.0f, 100.0f), PlayerNames::PLAYER_TWO);
	// a mobile object of the creature class, so that its body is the hooks' and needs no model; marked for deletion, so
	// that taking it out of the physics puts nothing back in the map's cells
	const auto thing = registry.Create();
	registry.Assign<Transform>(thing, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MobileObject>(thing);
	registry.Assign<Creature>(thing);
	registry.Assign<Unavailable>(thing);

	CreatureObjectActionSystem::ReleaseHeld(thing, glm::vec3(120.0f, 0.0f, 90.0f), glm::vec3(30.0f, 5.0f, 0.0f), creature,
	                                        true);

	// the game hands the physics the creature as the thrower and its player's interface
	const auto* entry = dynamics.Find(thing);
	ASSERT_NE(entry, nullptr);
	EXPECT_EQ(entry->thrower, creature);
	EXPECT_EQ(entry->throwerPlayer, std::optional(PlayerNames::PLAYER_TWO));
	// not a throw from the hand
	EXPECT_EQ(entry->flags & ecs::PhysicsEntry::k_FromHand, 0u);
	dynamics.RemoveObject(thing);
}

TEST_F(CreaturePhysicsTest, WhatACreatureThrowsAtATargetIsPutDownWhereItIsWhenNoBodyCanBeMadeForIt)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto rock = registry.Create();
	registry.Assign<Transform>(rock, glm::vec3(110.0f, 0.0f, 100.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	registry.Assign<MobileObject>(rock, MobileObjectInfo::Ball);
	_world.teleported.clear();

	// the rock has no mesh, so the physics can make it no body
	CreatureObjectActionSystem::ReleaseHeld(rock, glm::vec3(120.0f, 0.0f, 90.0f), glm::vec3(30.0f, 5.0f, 0.0f), creature, true);

	ASSERT_EQ(_world.teleported.size(), 1u);
	EXPECT_EQ(_world.teleported.front(), rock);
	EXPECT_EQ(Locator::dynamicsSystem::value().Find(rock), nullptr);
	EXPECT_EQ(registry.Get<Transform>(rock).position, glm::vec3(120.0f, 0.0f, 90.0f));
	EXPECT_TRUE(ecs::map_cells::IsObjectInMap(rock));
}
} // namespace
