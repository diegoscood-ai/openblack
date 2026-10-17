/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Animals as the creatures' food and targets: their worth to eat, what a creature knows of one, whether it may be picked
// up, and the mind's scans finding them. Also what frightens a creature (what it runs away from), and what it knows of a
// villager's or an abode's fire

#define LOCATOR_IMPLEMENTATIONS

#include <cstdlib>
#include <cstring>

#include <algorithm>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <PackFile.h>
#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureDesires.h"
#include "Creature/CreatureMindModel.h"
#include "Creature/CreatureMindTables.h"
#include "Creature/CreaturePlanActions.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureObjectAction.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/Components/Villager.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/Implementations/CreatureMindSystemDetail.h"
#include "ECS/Systems/Implementations/CreatureObjectActionSystem.h"
#include "InfoConstants.h"
#include "creature/CreatureSystemWorld.h"
#include "support/FireFakes.h"
#include "support/RestoreService.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureObjectActionSystem;
namespace mind_detail = openblack::ecs::systems::mind_detail;
using creature_plan_actions::Target;

namespace
{
/// The animals' service as far as the mind asks it: whether an animal frightens a creature, as the test says
class FakeAnimals final: public ecs::systems::AnimalSystemInterface
{
public:
	std::function<bool(entt::entity)> frightening = [](entt::entity) { return false; };
	std::function<bool(entt::entity)> creatureCanPickUp = [](entt::entity) { return false; };

	[[nodiscard]] float VisualTime() const override { return 0.0f; }
	void SetVisualTime(float /*hours*/) override {}
	[[nodiscard]] uint32_t AddDeathListener(DeathCallback /*callback*/) override { return 0; }
	void RemoveDeathListener(uint32_t /*id*/) override {}
	[[nodiscard]] const DeathListeners& GetDeathListeners() const override { return _listeners; }
	[[nodiscard]] uint32_t SingleSlotId() const override { return 0; }
	void SetSingleSlotId(uint32_t /*id*/) override {}
	void SetSpeciesDying(std::size_t /*species*/, DeathCallback /*dying*/) override {}
	[[nodiscard]] const DeathCallback* SpeciesDying(std::size_t /*species*/) const override { return nullptr; }
	void SetScale(entt::entity /*animal*/, float /*scale*/) override {}
	[[nodiscard]] float RadiusOf(entt::entity /*animal*/) const override { return 0.0f; }
	[[nodiscard]] entt::entity LeaderOf(entt::entity /*flock*/) const override { return entt::null; }
	[[nodiscard]] std::vector<entt::entity> MembersOf(entt::entity /*flock*/) const override { return {}; }
	[[nodiscard]] glm::vec2 GoalOf(entt::entity /*animal*/) const override { return glm::vec2(0.0f); }
	[[nodiscard]] float GoalHeightOf(entt::entity /*animal*/) const override { return 0.0f; }
	[[nodiscard]] bool IsFrighteningToCreature(entt::entity animal) const override { return frightening(animal); }
	[[nodiscard]] bool CanPlayerPickUp(entt::entity /*animal*/) const override { return false; }
	[[nodiscard]] bool CanBePickedUpByCreature(entt::entity animal) const override { return creatureCanPickUp(animal); }
	[[nodiscard]] bool CanBeStompedOnByCreature(entt::entity /*animal*/, float /*creatureHeight*/) const override
	{
		return false;
	}
	entt::entity CreateScriptFlock(int32_t /*id*/, glm::vec2 /*position*/, glm::vec2 /*home*/, float /*reach*/,
	                               float /*flockDistance*/) override
	{
		return entt::null;
	}
	[[nodiscard]] entt::entity FindScriptFlock(int32_t /*id*/) const override { return entt::null; }
	entt::entity CreateBird(AnimalInfo /*type*/, glm::vec2 /*position*/, uint32_t /*age*/, entt::entity /*flock*/) override
	{
		return entt::null;
	}

private:
	DeathListeners _listeners;
};

class CreatureAnimalFoodTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		_fires = &static_cast<test::FakeFires&>(Locator::fireSystem::emplace<test::FakeFires>());
		_animals = &static_cast<FakeAnimals&>(Locator::animalSystem::emplace<FakeAnimals>());
		auto& cow = _world.Info().animal.at(static_cast<size_t>(AnimalInfo::Cow));
		cow.foodValue = 30.0f;
		cow.playerCanPickUp = 1;
		_world.Info().animal.at(static_cast<size_t>(AnimalInfo::Lion)).playerCanPickUp = 0;
		// the mind finds food through the creatures' hands
		_hands = &static_cast<CreatureObjectActionSystem&>(
		    Locator::creatureObjectActionSystem::emplace<CreatureObjectActionSystem>());
	}

	static entt::entity MakeAnimal(AnimalInfo type, glm::vec3 position, int32_t player = -1)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Animal>(entity, ecs::components::Animal {.type = type, .age = 0, .player = player});
		registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	static entt::entity MakeSpell(glm::vec3 position)
	{
		auto& registry = test::creature_world::World::Registry();
		const auto entity = registry.Create();
		registry.Assign<Spell>(entity).position = position;
		return entity;
	}

	// put back after the registry, which the entities may reach as they go
	const test::RestoreService<Locator::fireSystem> _restoreFires;
	const test::RestoreService<Locator::animalSystem> _restoreAnimals;
	test::creature_world::World _world;
	const test::RestoreService<Locator::creatureObjectActionSystem> _restoreHands;
	CreatureObjectActionSystem* _hands {nullptr};
	test::FakeFires* _fires {nullptr};
	FakeAnimals* _animals {nullptr};
};
} // namespace

TEST_F(CreatureAnimalFoodTest, AnAnimalIsWorthItsFoodValue)
{
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f));
	EXPECT_EQ(_hands->FoodValueOf(cow), std::optional(30.0f));
	// a kind worth nothing to eat is no food
	EXPECT_FALSE(_hands->FoodValueOf(MakeAnimal(AnimalInfo::Wolf, glm::vec3(0.0f))).has_value());
}

TEST_F(CreatureAnimalFoodTest, AnAnimalIsNotPickedUpByWhetherAHandMayHoldIt)
{
	// The creature's own test decides, against its reach and weight (creature_pick_up, test_creature_pick_up_rules),
	// not the hand's: without its species' meshes a creature has no reach, so neither a cow a hand may hold nor a lion
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(_hands->CanPickUp(self, MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f))));
	EXPECT_FALSE(_hands->CanPickUp(self, MakeAnimal(AnimalInfo::Lion, glm::vec3(0.0f))));
	EXPECT_FALSE(CreatureObjectActionSystem::PickUpReach(self).has_value());
}

TEST_F(CreatureAnimalFoodTest, ABirdIsPickedUpByItsOwnTest)
{
	// whether a hand may hold doves does not count: the animal system's test of a bird does
	_world.Info().animal.at(static_cast<size_t>(AnimalInfo::Dove)).playerCanPickUp = 1;
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto dove = MakeAnimal(AnimalInfo::Dove, glm::vec3(0.0f));
	EXPECT_FALSE(_hands->CanPickUp(self, dove));
	_animals->creatureCanPickUp = [dove](entt::entity animal) { return animal == dove; };
	EXPECT_TRUE(_hands->CanPickUp(self, dove));
}

TEST_F(CreatureAnimalFoodTest, WhatACreatureKnowsOfAnAnimal)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f), PlayerNames::PLAYER_ONE);
	const auto wild = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f));
	const auto mine = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f), 0);
	const auto theirs = MakeAnimal(AnimalInfo::Cow, glm::vec3(0.0f), 2);
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	using creature_tree::Attribute;
	const auto belief = mind_detail::BeliefOf(registry, wild, self);
	ASSERT_TRUE(belief.has_value());
	EXPECT_EQ(belief->type, creature_tree::belief_types::k_Animal);
	EXPECT_EQ(belief->Value(Attribute::Animate), 1u);
	EXPECT_EQ(belief->Value(Attribute::Life), 1u);
	// nobody's, the creature's own player's, another's
	EXPECT_EQ(belief->Value(Attribute::Allegiance), 1u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, mine, self)->Value(Attribute::Allegiance), 0u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, theirs, self)->Value(Attribute::Allegiance), 2u);
	EXPECT_EQ(mind_detail::BeliefOf(registry, theirs, self)->Value(Attribute::PlayerNumber), 2u);
}

TEST_F(CreatureAnimalFoodTest, LiveFoodIsWhatTheCreatureCanEat)
{
	// The game's eating plans eat an animal they can pick up: here only a dove and a crow can be picked up (the animal
	// system's test of a bird), a cow can't (no reach without its species' meshes), whatever its food value. A crow is
	// worth no food, and is food all the same
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto dove = MakeAnimal(AnimalInfo::Dove, glm::vec3(10.0f, 0.0f, 0.0f));
	const auto crow = MakeAnimal(AnimalInfo::Crow, glm::vec3(8.0f, 0.0f, 0.0f));
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(5.0f, 0.0f, 0.0f));
	_animals->creatureCanPickUp = [dove, crow](entt::entity animal) { return animal == dove || animal == crow; };
	auto& registry = test::creature_world::World::Registry();
	const auto& lookup = std::as_const(registry);
	EXPECT_FALSE(_hands->FoodValueOf(crow).has_value());
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::LiveFood, dove, self));
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::LiveFood, crow, self));
	EXPECT_FALSE(mind_detail::Accepts(lookup, Target::LiveFood, cow, self));
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::Living, cow, self));
	// what it has found not to be food is no bar: the game's plans do not ask (the made-up dove row is of the game's
	// type Animal, as the game's own rows are)
	_world.Info().animal.at(static_cast<size_t>(AnimalInfo::Dove)).type = ObjectType::Animal;
	auto& mind = registry.Get<CreatureMindState>(self);
	mind.learnt = creature_mind_model::Learnt {};
	mind.learnt->objectTypes.at(static_cast<size_t>(ObjectType::Animal)).notFood = 1;
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::LiveFood, dove, self));
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::Food, dove, self));
	EXPECT_TRUE(mind_detail::CanBeEaten(lookup, self, dove));
	// a creature is never eaten
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(3.0f, 0.0f, 0.0f));
	EXPECT_FALSE(mind_detail::CanBeEaten(lookup, self, other));
}

TEST_F(CreatureAnimalFoodTest, OnlyMagicFoodAwayFromAStoragePitIsAPileToEatFrom)
{
	// The game's plan of eating from a pile takes a pile of magic food, here with no town near to have a pit; neither
	// another pot of food nor a pile another creature holds
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	auto& registry = test::creature_world::World::Registry();
	const auto pot = [&registry](PotInfo type, glm::vec3 at) {
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
		registry.Assign<Pot>(entity, Pot {.amount = 2500, .maxAmount = 10000, .type = type});
		return entity;
	};
	const auto magic = pot(PotInfo::MagicFood, glm::vec3(20.0f, 0.0f, 0.0f));
	const auto plain = pot(PotInfo::FoodPile, glm::vec3(10.0f, 0.0f, 0.0f));
	const auto held = pot(PotInfo::MagicFood, glm::vec3(5.0f, 0.0f, 0.0f));
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(3.0f, 0.0f, 0.0f));
	registry.Assign<HeldByCreature>(held, HeldByCreature {.creature = other});
	const auto& lookup = std::as_const(registry);
	EXPECT_TRUE(mind_detail::Accepts(lookup, Target::FoodPile, magic, self));
	EXPECT_FALSE(mind_detail::Accepts(lookup, Target::FoodPile, plain, self));
	EXPECT_FALSE(mind_detail::Accepts(lookup, Target::FoodPile, held, self));
	const auto found = mind_detail::Gather(lookup, Target::FoodPile, self, {0.0f, 0.0f});
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found.front().entity, magic);
}

TEST_F(CreatureAnimalFoodTest, TheScansFindWhatTheCreatureCanEat)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto dove = MakeAnimal(AnimalInfo::Dove, glm::vec3(10.0f, 0.0f, 0.0f));
	MakeAnimal(AnimalInfo::Cow, glm::vec3(5.0f, 0.0f, 0.0f));
	_animals->creatureCanPickUp = [dove](entt::entity animal) { return animal == dove; };
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	// the nearer cow can't be picked up here, so the dove is the food found
	const auto found = mind_detail::Gather(registry, Target::Food, self, glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found.front().entity, dove);
	const auto nearest = mind_detail::NearestFood(registry, self, glm::vec2(0.0f), {});
	ASSERT_TRUE(nearest.has_value());
	EXPECT_EQ(nearest->first, dove);
	EXPECT_EQ(nearest->second, glm::vec2(10.0f, 0.0f));
}

TEST_F(CreatureAnimalFoodTest, ACreatureIsFrightenedOfCreaturesSpellsAndFrighteningAnimals)
{
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto other = test::creature_world::World::MakeCreature(glm::vec3(20.0f, 0.0f, 0.0f));
	const auto bat = MakeAnimal(AnimalInfo::Bat, glm::vec3(5.0f, 0.0f, 0.0f));
	const auto cow = MakeAnimal(AnimalInfo::Cow, glm::vec3(6.0f, 0.0f, 0.0f));
	const auto spell = MakeSpell(glm::vec3(7.0f, 0.0f, 0.0f));
	const auto going = MakeSpell(glm::vec3(8.0f, 0.0f, 0.0f));
	auto& registry = test::creature_world::World::Registry();
	registry.Assign<Unavailable>(going);
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	registry.Assign<Transform>(villager, glm::vec3(9.0f, 0.0f, 0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	_animals->frightening = [bat](entt::entity animal) { return animal == bat; };

	const auto& view = std::as_const(registry);
	EXPECT_TRUE(mind_detail::Accepts(view, Target::Frightening, other, self));
	EXPECT_TRUE(mind_detail::Accepts(view, Target::Frightening, spell, self));
	EXPECT_TRUE(mind_detail::Accepts(view, Target::Frightening, bat, self));
	EXPECT_FALSE(mind_detail::Accepts(view, Target::Frightening, cow, self));
	EXPECT_FALSE(mind_detail::Accepts(view, Target::Frightening, villager, self));
	EXPECT_FALSE(mind_detail::Accepts(view, Target::Frightening, self, self));
	EXPECT_FALSE(mind_detail::Accepts(view, Target::Frightening, going, self));
	// without the animals' service no animal frightens it
	Locator::animalSystem::reset();
	EXPECT_FALSE(mind_detail::Accepts(view, Target::Frightening, bat, self));
}

TEST_F(CreatureAnimalFoodTest, TheFrighteningScanOffersNoSpell)
{
	// a spell passes the test, but how one is offered to it is not known, so no scan offers one
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	const auto bat = MakeAnimal(AnimalInfo::Bat, glm::vec3(10.0f, 0.0f, 0.0f));
	const auto spell = MakeSpell(glm::vec3(0.0f, 3.0f, 4.0f));
	MakeAnimal(AnimalInfo::Cow, glm::vec3(1.0f, 0.0f, 0.0f));
	_animals->frightening = [bat](entt::entity animal) { return animal == bat; };
	const auto& registry = std::as_const(test::creature_world::World::Registry());

	const auto found = mind_detail::Gather(registry, Target::Frightening, self, glm::vec2(0.0f));
	ASSERT_EQ(found.size(), 1u);
	EXPECT_EQ(found.at(0).entity, bat);
	const auto anything = mind_detail::Gather(registry, Target::Anything, self, glm::vec2(0.0f));
	EXPECT_TRUE(std::ranges::none_of(anything, [spell](const mind_detail::Found& f) { return f.entity == spell; }));
}

TEST(CreatureFrightening, ACreatureRunsAwayFromWhatFrightensIt)
{
	const auto* executor = creature_plan_actions::For("RunAwayFromObject");
	ASSERT_NE(executor, nullptr);
	EXPECT_EQ(executor->target, Target::Frightening);
}

TEST(CreatureFrightening, OnFireIsZeroAndNotOnFireOne)
{
	static_assert(mind_detail::OnFireValue(true) == 0);
	static_assert(mind_detail::OnFireValue(false) == 1);
}

TEST_F(CreatureAnimalFoodTest, ACreatureKnowsWhetherAVillagerOrAnAbodeBurns)
{
	using creature_tree::Attribute;
	const auto self = test::creature_world::World::MakeCreature(glm::vec3(0.0f));
	auto& registry = test::creature_world::World::Registry();
	const auto villager = registry.Create();
	registry.Assign<Villager>(villager);
	const auto abode = registry.Create();
	registry.Assign<Abode>(abode);
	const auto& view = std::as_const(registry);
	EXPECT_EQ(mind_detail::BeliefOf(view, villager, self)->Value(Attribute::OnFire), 1u);
	EXPECT_EQ(mind_detail::BeliefOf(view, abode, self)->Value(Attribute::OnFire), 1u);
	// a fire below the lowest combustion temperature (40) does not burn yet
	_fires->Burn(villager, 39.0f);
	EXPECT_EQ(mind_detail::BeliefOf(view, villager, self)->Value(Attribute::OnFire), 1u);
	_fires->Burn(villager, 1000.0f);
	_fires->Burn(abode, 1000.0f);
	EXPECT_EQ(mind_detail::BeliefOf(view, villager, self)->Value(Attribute::OnFire), 0u);
	EXPECT_EQ(mind_detail::BeliefOf(view, abode, self)->Value(Attribute::OnFire), 0u);
}

// Integration test: needs the original game data (OPENBLACK_GAME_PATH); skipped without it
TEST(CreatureFrightening, OnlyFearListsRunningAwayFromAnObject)
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		GTEST_SKIP() << "OPENBLACK_GAME_PATH not set";
	}
	// Scripts/info.dat's "Info" block, either version, as InfoFile reads it (without the Locator)
	pack::PackFile pack;
	if (pack.Open(std::filesystem::path(game) / "Scripts" / "info.dat") != pack::PackResult::Success || !pack.HasBlock("Info"))
	{
		GTEST_SKIP() << "no Info block in Scripts/info.dat";
	}
	const auto& data = pack.GetBlock("Info");
	auto info = std::make_unique<InfoConstants>();
	if (data.size() == sizeof(v100::InfoConstants))
	{
		auto old = std::make_unique<v100::InfoConstants>();
		std::memcpy(old.get(), data.data(), sizeof(v100::InfoConstants));
		UpdateInfo(*info, *old);
	}
	else if (data.size() == sizeof(v120::InfoConstants))
	{
		std::memcpy(info.get(), data.data(), sizeof(v120::InfoConstants));
	}
	else
	{
		GTEST_SKIP() << "the Info block is neither version 1.0 nor 1.2: " << data.size() << " bytes";
	}
	const auto tables = creature_mind_tables::Build(*info);

	const auto names = [&tables](size_t desire) {
		std::vector<std::string> listed;
		for (const auto action : tables.desireActions.at(desire))
		{
			listed.push_back(tables.actions.at(action).name);
		}
		return listed;
	};
	const auto fear = static_cast<size_t>(creature_desires::Desire::Fear);
	EXPECT_EQ(names(fear), (std::vector<std::string> {"RunAwayFromObject", "BeFrightenedOnTheSpot", "RunHome"}));
	for (size_t d = 0; d < creature_desires::k_DesireCount; ++d)
	{
		if (d != fear)
		{
			EXPECT_EQ(std::ranges::count(names(d), std::string("RunAwayFromObject")), 0) << d;
		}
	}
	// Compassion's row of the game's table (the heal, putting out a fire with water and others) is never planned
	EXPECT_TRUE(names(static_cast<size_t>(creature_desires::Desire::Compassion)).empty());
	EXPECT_FALSE(names(static_cast<size_t>(creature_desires::Desire::Anger)).empty());
}

TEST(CreatureMindTables, CompassionsOwnRowIsNeverPlanned)
{
	auto info = std::make_unique<InfoConstants>();
	// the same action in the rows of compassion and of anger
	constexpr uint32_t k_Action = 5;
	const auto compassion = static_cast<size_t>(creature_desires::Desire::Compassion);
	const auto anger = static_cast<size_t>(creature_desires::Desire::Anger);
	std::memcpy(&info->creatureDesireAction1.at(compassion), &k_Action, sizeof(k_Action));
	std::memcpy(&info->creatureDesireAction1.at(anger), &k_Action, sizeof(k_Action));
	const auto tables = creature_mind_tables::Build(*info);
	EXPECT_TRUE(tables.desireActions.at(compassion).empty());
	EXPECT_EQ(tables.desireActions.at(anger), (std::vector<uint32_t> {k_Action}));
}

TEST(CreatureMindTables, EachOfATownsDesiresListsItsActionsToTheFirstEmptySlot)
{
	auto info = std::make_unique<InfoConstants>();
	// the town's desire 4: two actions, an empty slot, then one more that is never read
	const std::array<uint32_t, 4> row {7, 3, 0, 9};
	std::memcpy(&info->creatureDesireAction2.at(4), row.data(), sizeof(row));
	// the town's desire 6: an action past the table's last ends the list too
	const std::array<uint32_t, 2> past {5, 100000};
	std::memcpy(&info->creatureDesireAction2.at(6), past.data(), sizeof(past));
	const auto tables = creature_mind_tables::Build(*info);
	EXPECT_EQ(tables.townActions.at(4), (std::vector<uint32_t> {7, 3}));
	EXPECT_EQ(tables.townActions.at(6), (std::vector<uint32_t> {5}));
	EXPECT_TRUE(tables.townActions.at(0).empty());
}
