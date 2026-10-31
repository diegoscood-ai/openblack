/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What a Living takes from a physics impact (villager_physics::ImpactDamage): the info's physics impact effect times
// (G - 2) x 0.03 strictly above 2 G, its positive crush and hit times the Living's own defence multipliers. Who the
// effect comes from (villager_physics::ImpactEffectOf, effects::EffectPlayer): what struck it, the thrower's hand, and
// whose alignment it moves. A hand-made info.dat, no game data. And what the life it loses or wins back does to its
// town's count of injured people.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <limits>
#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "ECS/Components/Animal.h"
#include "ECS/Components/Indestructible.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Life.h"
#include "ECS/PhysicsEntry.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/VillagerPhysics.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/EffectSourceRules.h"
#include "Physics/LivingRules.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_physics = openblack::ecs::villager_physics;
namespace living = openblack::physics::living;
namespace life = openblack::ecs::life;
namespace effects = openblack::ecs::effects;

namespace
{
constexpr auto k_Animal = static_cast<AnimalInfo>(0);

class LivingImpactTest: public ::testing::Test
{
protected:
	void SetUp() override { Locator::entitiesRegistry::emplace<ecs::Registry>(); }

	void TearDown() override
	{
		Locator::entitiesRegistry::reset();
		Locator::infoConstants::reset();
	}

	/// The physics impact effect with this crush, hit and heal; the animal's crush and hit multipliers
	static void SetInfo(float crush, float hit, float heal, float crushDefence, float hitDefence)
	{
		auto info = std::make_unique<InfoConstants>();
		auto& effect = info->effect.at(3);
		effect.effectCrush = crush;
		effect.effectHit = hit;
		effect.effectHeal = heal;
		auto& animal = info->animal.at(static_cast<size_t>(k_Animal));
		animal.defenceMultiplierBurn = 1.0f;
		animal.defenceMultiplierCrush = crushDefence;
		animal.defenceMultiplierHit = hitDefence;
		Locator::infoConstants::reset(info.release());
	}

	static entt::entity MakeAnimal()
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto animal = registry.Create();
		registry.Assign<Animal>(animal, Animal {.type = k_Animal});
		return animal;
	}
};
} // namespace

TEST_F(LivingImpactTest, TheCrushCountsByTheAnimalsOwnMultiplier)
{
	SetInfo(1.0f, 0.0f, 0.0f, 2.0f, 4.0f);
	const auto animal = MakeAnimal();
	const auto damage = villager_physics::ImpactDamage(animal, 4.0f);
	ASSERT_TRUE(damage.has_value());
	// (4 - 2) x 0.03 = 0.06, x 1 crush, x 2
	const float scale = (4.0f - 2.0f) * 0.03f;
	EXPECT_EQ(*damage, scale * 2.0f);
}

TEST_F(LivingImpactTest, AHitInTheEffectCountsTooANegativeOneNot)
{
	SetInfo(1.0f, 0.5f, 0.0f, 2.0f, 4.0f);
	const auto animal = MakeAnimal();
	const float scale = (3.0f - 2.0f) * 0.03f;
	const auto damage = villager_physics::ImpactDamage(animal, 3.0f);
	ASSERT_TRUE(damage.has_value());
	EXPECT_FLOAT_EQ(*damage, scale * 2.0f + scale * 0.5f * 4.0f);
	SetInfo(1.0f, -0.5f, 0.0f, 2.0f, 4.0f);
	EXPECT_FLOAT_EQ(*villager_physics::ImpactDamage(animal, 3.0f), scale * 2.0f);
}

TEST_F(LivingImpactTest, WithoutTheInfoTheCrushIsOne)
{
	const auto animal = MakeAnimal();
	const auto damage = villager_physics::ImpactDamage(animal, 2.5f);
	ASSERT_TRUE(damage.has_value());
	EXPECT_EQ(*damage, (2.5f - 2.0f) * 0.03f);
}

TEST(LivingCrush, OnlyAKnockStrictlyAboveTwiceItsWeightHurts)
{
	EXPECT_EQ(living::k_HurtingKnock, 2.0f);
	EXPECT_EQ(living::k_CrushPerKnock, 0.03f);
	// at 2 G exactly, below it, or a knock that compares with nothing: no effect at all
	EXPECT_FALSE(living::LivingCrush(2.0f).has_value());
	EXPECT_FALSE(living::LivingCrush(1.0f).has_value());
	EXPECT_FALSE(living::LivingCrush(std::nanf("")).has_value());
	// the next float above 2 hurts, by its tiny share
	const float above = std::nextafter(2.0f, 3.0f);
	ASSERT_TRUE(living::LivingCrush(above).has_value());
	EXPECT_GT(*living::LivingCrush(above), 0.0f);
	EXPECT_EQ(*living::LivingCrush(above), (above - 2.0f) * 0.03f);
	// the share, worked out in floats
	EXPECT_EQ(*living::LivingCrush(4.0f), 2.0f * 0.03f);
	EXPECT_EQ(*living::LivingCrush(12.5f), (12.5f - 2.0f) * 0.03f);
}

TEST(CreatureCrush, ACreaturesKnockIsItsWeightsShareUpToAHundredAndCountsAboveAFifthOfAHundredth)
{
	// a creature of size 1, neither fat nor thin, weak nor strong: 100 x (8.333334)^3
	const float mass = living::CreatureMass(1.0f, 0.0f, 0.0f);
	EXPECT_NEAR(mass, 57870.4f, 0.1f);
	// fat and strong weigh more: (1 + 2 x 0.15) times as much
	EXPECT_NEAR(living::CreatureMass(1.0f, 1.0f, 1.0f), mass * 1.3f, 0.1f);
	// twice the size, eight times the mass
	EXPECT_NEAR(living::CreatureMass(2.0f, 0.0f, 0.0f), mass * 8.0f, 1.0f);

	const float weight = mass * 9.81f;
	// fifty times its weight: half of the crush
	ASSERT_TRUE(living::CreatureCrush(weight * 50.0f, mass).has_value());
	EXPECT_NEAR(*living::CreatureCrush(weight * 50.0f, mass), 0.5f, 1e-5f);
	// at most a hundred times its weight, and a hundred for a knock that compares with nothing
	EXPECT_EQ(*living::CreatureCrush(weight * 400.0f, mass), 100.0f * 0.01f);
	EXPECT_EQ(*living::CreatureCrush(std::nanf(""), mass), 100.0f * 0.01f);
	// half of its weight is 0.005 of the crush, which does not count: it must be more
	EXPECT_FALSE(living::CreatureCrush(weight * 0.5f, mass).has_value());
	EXPECT_FALSE(living::CreatureCrush(0.0f, mass).has_value());
}

TEST(CarriedShare, WhatACreatureCarriesCountsAsItsShareOfTheCreaturesWeightUpToAll)
{
	// a creature of size 1 carrying a fifth of its weight
	const float mass = living::CreatureMass(1.0f, 0.0f, 0.0f);
	EXPECT_EQ(living::CarriedShare(mass * 0.2f, mass), (mass * 0.2f) / mass);
	EXPECT_EQ(living::CarriedShare(0.0f, mass), 0.0f);
	// as heavy as itself is all of it, and heavier counts as all of it
	EXPECT_EQ(living::CarriedShare(mass, mass), 1.0f);
	EXPECT_EQ(living::CarriedShare(mass * 3.0f, mass), 1.0f);
	EXPECT_EQ(living::CarriedShare(std::numeric_limits<float>::infinity(), mass), 1.0f);
	// a share below nothing, or one that compares with nothing, counts as nothing
	EXPECT_EQ(living::CarriedShare(-5.0f, mass), 0.0f);
	EXPECT_EQ(living::CarriedShare(std::nanf(""), mass), 0.0f);
	// a creature that weighs nothing, or whose weight compares with nothing, carries nothing
	EXPECT_EQ(living::CarriedShare(10.0f, 0.0f), 0.0f);
	EXPECT_EQ(living::CarriedShare(10.0f, -0.0f), 0.0f);
	EXPECT_EQ(living::CarriedShare(10.0f, std::nanf("")), 0.0f);
	// a smaller creature carries the same thing as a bigger share of itself: half the size, an eighth of the mass
	const float small = living::CreatureMass(0.5f, 0.0f, 0.0f);
	EXPECT_NEAR(living::CarriedShare(mass * 0.1f, small), 0.8f, 1e-5f);
}

TEST_F(LivingImpactTest, AtTwiceItsWeightAnImpactTakesNothing)
{
	SetInfo(1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	const auto animal = MakeAnimal();
	EXPECT_FALSE(villager_physics::ImpactDamage(animal, 2.0f).has_value());
	EXPECT_FALSE(villager_physics::ImpactEffectOf(animal, {.g = 2.0f}).has_value());
	const float above = std::nextafter(2.0f, 3.0f);
	ASSERT_TRUE(villager_physics::ImpactDamage(animal, above).has_value());
	EXPECT_EQ(*villager_physics::ImpactDamage(animal, above), (above - 2.0f) * 0.03f);
}

TEST(EffectPlayer, WhatAppliesItAnswersForItselfElseTheThrowerElseTheNeutralPlayer)
{
	// an applier with a player: that player, whoever threw it
	EXPECT_EQ(effects::EffectPlayer(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_TWO);
	// an applier with no player: none, even with a thrower
	EXPECT_FALSE(effects::EffectPlayer(true, std::nullopt, PlayerNames::PLAYER_ONE).has_value());
	// no applier: the thrower
	EXPECT_EQ(effects::EffectPlayer(false, std::nullopt, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_ONE);
	// no applier and no thrower: the neutral player
	EXPECT_EQ(effects::EffectPlayer(false, std::nullopt, std::nullopt), PlayerNames::NEUTRAL);
}

TEST_F(LivingImpactTest, AnImpactsEffectComesFromWhatStruckItAndTheHandThatThrewIt)
{
	SetInfo(1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = MakeAnimal();
	const float share = (4.0f - 2.0f) * 0.03f;
	constexpr auto k_Crush = static_cast<size_t>(effects::EffectValues::Number::Crush);

	// a rock thrown by the hand strikes it: the rock applies it, the hand's player is the caused one (the town's
	// attack takes it first), and the rock answers for the alignment with its own player, the neutral one
	const auto rock = registry.Create();
	const auto byRock =
	    villager_physics::ImpactEffectOf(animal, {.g = 4.0f, .hitBy = rock, .throwerPlayer = PlayerNames::PLAYER_ONE});
	ASSERT_TRUE(byRock.has_value());
	EXPECT_EQ(byRock->numbers.at(k_Crush), share);
	EXPECT_EQ(byRock->appliedBy, rock);
	EXPECT_FALSE(byRock->appliedByCreature);
	ASSERT_TRUE(byRock->causedPlayer.has_value());
	EXPECT_EQ(*byRock->causedPlayer, PlayerNames::PLAYER_ONE);
	ASSERT_TRUE(byRock->player.has_value());
	EXPECT_EQ(*byRock->player, PlayerNames::NEUTRAL);

	// a villager of a town strikes it: its town's owner
	const auto town = registry.Create();
	registry.Assign<Town>(town, 1u).owner = PlayerNames::PLAYER_TWO;
	const auto townsman = registry.Create();
	registry.Assign<Villager>(townsman).town = town;
	const auto byTownsman = villager_physics::ImpactEffectOf(animal, {.g = 4.0f, .hitBy = townsman});
	ASSERT_TRUE(byTownsman.has_value());
	EXPECT_FALSE(byTownsman->causedPlayer.has_value());
	ASSERT_TRUE(byTownsman->player.has_value());
	EXPECT_EQ(*byTownsman->player, PlayerNames::PLAYER_TWO);

	// a villager of no town strikes it: no player, so no alignment moves, though the hand threw it
	const auto vagrant = registry.Create();
	registry.Assign<Villager>(vagrant).town = entt::null;
	const auto byVagrant =
	    villager_physics::ImpactEffectOf(animal, {.g = 4.0f, .hitBy = vagrant, .throwerPlayer = PlayerNames::PLAYER_ONE});
	ASSERT_TRUE(byVagrant.has_value());
	EXPECT_FALSE(byVagrant->player.has_value());

	// it falls onto the land from the hand: nothing applies it, the hand's player
	const auto thrown = villager_physics::ImpactEffectOf(animal, {.g = 4.0f, .throwerPlayer = PlayerNames::PLAYER_ONE});
	ASSERT_TRUE(thrown.has_value());
	EXPECT_EQ(thrown->appliedBy, entt::entity {entt::null});
	ASSERT_TRUE(thrown->player.has_value());
	EXPECT_EQ(*thrown->player, PlayerNames::PLAYER_ONE);

	// it falls with no hand and nothing striking it: the neutral player
	const auto fell = villager_physics::ImpactEffectOf(animal, {.g = 4.0f});
	ASSERT_TRUE(fell.has_value());
	EXPECT_FALSE(fell->causedPlayer.has_value());
	ASSERT_TRUE(fell->player.has_value());
	EXPECT_EQ(*fell->player, PlayerNames::NEUTRAL);

	// a hitter already gone counts as nothing striking it
	registry.Destroy(rock);
	const auto byGone = villager_physics::ImpactEffectOf(animal, {.g = 4.0f, .hitBy = rock});
	ASSERT_TRUE(byGone.has_value());
	EXPECT_EQ(byGone->appliedBy, entt::entity {entt::null});
	ASSERT_TRUE(byGone->player.has_value());
	EXPECT_EQ(*byGone->player, PlayerNames::NEUTRAL);

	// every one of them already says who applied it, so the miracles' service, which the impact goes through, takes
	// the values as they are and does not fill them from the blow's source
	for (const auto* values : {&*byRock, &*byTownsman, &*byVagrant, &*thrown, &*fell, &*byGone})
	{
		EXPECT_FALSE(openblack::magic::NamesNoApplier(*values));
	}
}

TEST_F(LivingImpactTest, AnAnimalOutOfReachInTheSameTurnStillTakesTheImpact)
{
	SetInfo(1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto animal = MakeAnimal();
	registry.Assign<Unavailable>(animal);
	const auto values = villager_physics::ImpactEffectOf(animal, {.g = 4.0f});
	ASSERT_TRUE(values.has_value());
	EXPECT_EQ(values->numbers.at(static_cast<size_t>(effects::EffectValues::Number::Crush)), (4.0f - 2.0f) * 0.03f);
}

TEST(LivingInjured, OnlyAStrictCrossingOfTheMarkCounts)
{
	EXPECT_EQ(living::k_InjuredLife, 0.7f);
	// falling below: one more; rising above: one fewer
	EXPECT_EQ(living::InjuredChange(0.8f, 0.6f), 1);
	EXPECT_EQ(living::InjuredChange(1.0f, 0.0f), 1);
	EXPECT_EQ(living::InjuredChange(0.6f, 0.8f), -1);
	EXPECT_EQ(living::InjuredChange(0.0f, 1.0f), -1);
	// staying on one side: nothing
	EXPECT_EQ(living::InjuredChange(0.9f, 0.75f), 0);
	EXPECT_EQ(living::InjuredChange(0.5f, 0.2f), 0);
	// to or from exactly the mark: nothing, either way
	EXPECT_EQ(living::InjuredChange(0.8f, 0.7f), 0);
	EXPECT_EQ(living::InjuredChange(0.7f, 0.6f), 0);
	EXPECT_EQ(living::InjuredChange(0.6f, 0.7f), 0);
	EXPECT_EQ(living::InjuredChange(0.7f, 0.8f), 0);
}

TEST_F(LivingImpactTest, AVillagersTownCountsItInjuredAsItsLifeCrossesTheMark)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town, 1u);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	registry.Get<Villager>(villager).life = 1.0f;
	registry.Get<Villager>(villager).town = town;
	const auto injured = [&registry, town] { return registry.Get<Town>(town).injured; };

	// every way the life is written goes through the setter
	life::SetLife(villager, 0.8f);
	EXPECT_EQ(injured(), 0);
	life::ReduceLife(villager, 0.2f);
	EXPECT_EQ(injured(), 1);
	life::SetLife(villager, 0.5f);
	EXPECT_EQ(injured(), 1);
	life::IncreaseLife(villager, 0.4f);
	EXPECT_EQ(injured(), 0);
	// through the mark itself: no change on either side of it
	life::SetLife(villager, living::k_InjuredLife);
	life::SetLife(villager, 0.6f);
	EXPECT_EQ(injured(), 0);
	// recovering is never kept from taking one off: the count goes below 0
	life::SetLife(villager, 0.9f);
	EXPECT_EQ(injured(), -1);
	life::SetLife(villager, 0.1f);
	EXPECT_EQ(injured(), 0);

	// a villager with no town counts nowhere
	registry.Get<Villager>(villager).town = entt::null;
	life::SetLife(villager, 1.0f);
	EXPECT_EQ(injured(), 0);
	EXPECT_EQ(life::LifeOf(villager), 1.0f);
	// an animal has no town to count it
	const auto animal = MakeAnimal();
	life::SetLife(animal, 0.1f);
	EXPECT_EQ(injured(), 0);
}

TEST_F(LivingImpactTest, AProtectedThingKeepsItsLifeAgainstAHundredthOrLessButItsTownStillCountsIt)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto town = registry.Create();
	registry.Assign<Town>(town, 1u);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	registry.Get<Villager>(villager).life = 1.0f;
	registry.Get<Villager>(villager).town = town;
	const auto injured = [&registry, town] { return registry.Get<Town>(town).injured; };

	// a hundredth is kept from, and so is not a number; the town counts him injured all the same
	life::SetLifeUnlessProtected(villager, 0.01f, true);
	EXPECT_EQ(life::LifeOf(villager), 1.0f);
	EXPECT_EQ(injured(), 1);
	life::SetLifeUnlessProtected(villager, std::numeric_limits<float>::quiet_NaN(), true);
	EXPECT_EQ(life::LifeOf(villager), 1.0f);
	// more than a hundredth is set
	life::SetLifeUnlessProtected(villager, 0.02f, true);
	EXPECT_EQ(life::LifeOf(villager), 0.02f);
	// unprotected, anything is set
	life::SetLifeUnlessProtected(villager, 0.0f, false);
	EXPECT_EQ(life::LifeOf(villager), 0.0f);

	// any other object the same, with no town to count it
	const auto animal = MakeAnimal();
	life::SetLife(animal, 0.5f);
	life::SetLifeUnlessProtected(animal, 0.005f, true);
	EXPECT_EQ(life::LifeOf(animal), 0.5f);
	life::SetLifeUnlessProtected(animal, 0.3f, true);
	EXPECT_EQ(life::LifeOf(animal), 0.3f);
}

TEST(LifeRules, AProtectedThingKeepsItsLifeOnlyThroughTheObjectsSetter)
{
	// the object's setter keeps a protected thing's life against a hundredth or less, or not a number; a tree is felled
	// by a life of none or less before it, a pile never reaches it (docs/wiki/scripts/script-commands.md,
	// SET_INDESTRUCTABLE)
	using life::LifeSetter;
	constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(life::KeepsLife(true, LifeSetter::Object, 0.01f));
	EXPECT_TRUE(life::KeepsLife(true, LifeSetter::Object, 0.0f));
	EXPECT_TRUE(life::KeepsLife(true, LifeSetter::Object, k_NaN));
	EXPECT_FALSE(life::KeepsLife(true, LifeSetter::Object, 0.02f));
	EXPECT_FALSE(life::KeepsLife(false, LifeSetter::Object, 0.0f));
	EXPECT_FALSE(life::KeepsLife(true, LifeSetter::Tree, 0.0f));
	EXPECT_FALSE(life::KeepsLife(true, LifeSetter::Tree, -1.0f));
	EXPECT_TRUE(life::KeepsLife(true, LifeSetter::Tree, 0.005f));
	EXPECT_TRUE(life::KeepsLife(true, LifeSetter::Tree, k_NaN));
	EXPECT_FALSE(life::KeepsLife(true, LifeSetter::Pile, 0.0f));
}

TEST_F(LivingImpactTest, AThingMarkedIndestructibleKeepsItsLifeWhereverItIsSet)
{
	// SetLife (the effects' damage through ReduceLife too) protects a thing marked indestructible, as the original's
	// object setter does wherever the life is set; a tree marked so is still brought to none
	auto& registry = Locator::entitiesRegistry::value();
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	registry.Get<Villager>(villager).life = 1.0f;
	registry.Get<Villager>(villager).town = entt::null;
	registry.Assign<Indestructible>(villager);
	life::SetLife(villager, 0.0f);
	EXPECT_EQ(life::LifeOf(villager), 1.0f);
	EXPECT_EQ(life::ReduceLife(villager, 2.0f), 1.0f);
	life::SetLife(villager, 0.5f);
	EXPECT_EQ(life::LifeOf(villager), 0.5f);
	registry.Remove<Indestructible>(villager);
	life::SetLife(villager, 0.0f);
	EXPECT_EQ(life::LifeOf(villager), 0.0f);

	const auto animal = MakeAnimal();
	registry.Assign<Indestructible>(animal);
	life::SetLife(animal, 0.5f);
	life::SetLife(animal, 0.0f);
	EXPECT_EQ(life::LifeOf(animal), 0.5f);

	const auto tree = registry.Create();
	registry.Assign<Tree>(tree);
	registry.Assign<Indestructible>(tree);
	life::SetLife(tree, 0.5f);
	life::SetLife(tree, 0.005f);
	EXPECT_EQ(life::LifeOf(tree), 0.5f);
	life::SetLife(tree, 0.0f);
	EXPECT_EQ(life::LifeOf(tree), 0.0f);
}
