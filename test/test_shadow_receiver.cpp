/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which objects receive the projected shadows that fall on objects (src/ECS/ShadowReceiver.h): every game object but
// the classes that turn the flag off, the bare 3D objects, and the creature, whose drawn body never asks for it.

#include <gtest/gtest.h>

#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/ShadowReceiver.h"

using openblack::PotInfo;
using openblack::ecs::shadow_receiver::Object;
using openblack::ecs::shadow_receiver::Receives;

TEST(ShadowReceiver, AnOrdinaryObjectReceives)
{
	EXPECT_TRUE(Receives({}));
	EXPECT_TRUE(Receives({.pot = PotInfo::FoodPile}));
	EXPECT_TRUE(Receives({.pot = PotInfo::HandWood}));
}

TEST(ShadowReceiver, ACreatureDoesNotReceive)
{
	EXPECT_FALSE(Receives({.creature = true}));
}

TEST(ShadowReceiver, TheClassesThatTurnTheFlagOffDoNotReceive)
{
	EXPECT_FALSE(Receives({.powerUpBand = true}));
	EXPECT_FALSE(Receives({.tree = true}));
	EXPECT_FALSE(Receives({.forest = true}));
	EXPECT_FALSE(Receives({.hand = true}));
	EXPECT_FALSE(Receives({.orb = true}));
	EXPECT_FALSE(Receives({.shield = true}));
	EXPECT_FALSE(Receives({.flowers = true}));
	EXPECT_FALSE(Receives({.templePart = true}));
	EXPECT_FALSE(Receives({.handFxPart = true}));
	EXPECT_FALSE(Receives({.desireFlag = true}));
}

TEST(ShadowReceiver, TheFoodInTheHandAndTheMagicFoodDoNotReceive)
{
	EXPECT_FALSE(Receives({.pot = PotInfo::HandFood}));
	EXPECT_FALSE(Receives({.pot = PotInfo::MagicFood}));
}

static_assert(Receives(Object {}) && !Receives(Object {.creature = true}));

// An entity's inputs to the rule come from its components: a tree does not receive, magic food does not, a plain
// thing does
TEST(ShadowReceiver, AnEntitysInputsComeFromItsComponents)
{
	openblack::ecs::Registry registry;
	const auto thing = registry.Create();
	EXPECT_TRUE(openblack::ecs::shadow_receiver::ReceivesOf(registry, thing));
	const auto tree = registry.Create();
	registry.Assign<openblack::ecs::components::Tree>(tree);
	EXPECT_FALSE(openblack::ecs::shadow_receiver::ReceivesOf(registry, tree));
	const auto food = registry.Create();
	registry.Assign<openblack::ecs::components::Pot>(food).type = openblack::PotInfo::MagicFood;
	EXPECT_FALSE(openblack::ecs::shadow_receiver::ReceivesOf(registry, food));
}
