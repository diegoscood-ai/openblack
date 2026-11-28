/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/HandDrawPose.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/ParticleDrawnMatrix.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Tree.h"
#include "ECS/OutOfMapDraw.h"
#include "ECS/Registry.h"
#include "ECS/SuperVillager.h"

namespace
{
namespace out_of_map_draw = openblack::ecs::out_of_map_draw;
namespace c = openblack::ecs::components;
using out_of_map_draw::Colours;
} // namespace

TEST(OutOfMapDraw, InTheMapOrWithItsOwnDrawAnObjectKeepsItsClasssColours)
{
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = false, .burning = true}), Colours::OwnDraw);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = false, .pot = true, .poisoned = true}), Colours::OwnDraw);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .ownDraw = true}), Colours::OwnDraw);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .ownDraw = true, .burning = true}), Colours::OwnDraw);
}

TEST(OutOfMapDraw, OutOfTheMapTheFireGivesItsPairElseTheLandLightAlone)
{
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true}), Colours::LandLight);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .burning = true}), Colours::Burning);
	// only a pot or a pile tests the poison: a poisoned villager out of the map takes the land light alone
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .poisoned = true}), Colours::LandLight);
}

TEST(OutOfMapDraw, APoisonedPotTakesThePoisonsPairOnlyWithoutAFire)
{
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .pot = true, .poisoned = true}), Colours::Poisoned);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .pot = true, .poisoned = true, .burning = true}), Colours::Burning);
	EXPECT_EQ(out_of_map_draw::ColoursOf({.outOfMap = true, .pot = true}), Colours::LandLight);
}

TEST(OutOfMapDraw, HeldFlyingAndCarriedObjectsAreDrawnOutOfTheMap)
{
	openblack::ecs::Registry registry;
	const auto standing = registry.Create();
	EXPECT_FALSE(out_of_map_draw::DrawnOutOfMap(registry, standing, false));
	// a computer player's hand's held thing, known to the renderer only
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, standing, true));
	const auto inHand = registry.Create();
	registry.Assign<c::HandDrawPose>(inHand);
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, inHand, false));
	const auto flying = registry.Create();
	registry.Assign<c::PhysicsDrawPose>(flying);
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, flying, false));
	const auto creatureHeld = registry.Create();
	registry.Assign<c::HeldByCreature>(creatureHeld);
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, creatureHeld, false));
	const auto tornado = registry.Create();
	registry.Assign<c::CarriedByTornado>(tornado);
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, tornado, false));
	const auto vortex = registry.Create();
	registry.Assign<c::ParticleDrawnMatrix>(vortex);
	EXPECT_TRUE(out_of_map_draw::DrawnOutOfMap(registry, vortex, false));
}

TEST(OutOfMapDraw, TreesSeedsOrbsCreaturesSuperVillagersAndShieldsKeepTheirOwnDraw)
{
	openblack::ecs::Registry registry;
	const auto plain = registry.Create();
	EXPECT_FALSE(out_of_map_draw::HasItsOwnDraw(registry, plain));
	const auto tree = registry.Create();
	registry.Assign<c::Tree>(tree);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, tree));
	const auto seed = registry.Create();
	registry.Assign<c::SpellSeed>(seed);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, seed));
	const auto orb = registry.Create();
	registry.Assign<c::OneOffSpellSeed>(orb);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, orb));
	const auto creature = registry.Create();
	registry.Assign<c::Creature>(creature);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, creature));
	const auto superVillager = registry.Create();
	registry.Assign<c::SuperVillager>(superVillager);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, superVillager));
	const auto shield = registry.Create();
	registry.Assign<c::MagicShield>(shield);
	EXPECT_TRUE(out_of_map_draw::HasItsOwnDraw(registry, shield));
}
