/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The age of an animal as its needs see it: Animal::ProcessNeeds, Cow::ReactToAnimalNeeds and Animal::IsChild ask
// Living::GetAge, (turn - birth turn) / 1500, every time. Then the needs themselves: the counters each turn and a
// grazer's sleep.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <memory>

#include <entt/entity/entity.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "ECS/AnimalAIDetail.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/WorldSystems.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace ai = openblack::ecs::animal_ai::detail;

namespace
{
constexpr uint32_t k_Created = 100000;

/// info.dat, the cow's GAnimalInfo: grownUpAge 13, hunger 50, sleep 1000, needToBreed 3000
GAnimalInfo CowInfo()
{
	GAnimalInfo info {};
	info.grownUpAge = 13;
	info.hunger = 50;
	info.sleep = 1000;
	info.needToBreed = 3000;
	return info;
}

class AnimalAgeTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		openblack::test::EmplaceWorldSystems();
		game_clock::SetTurn(k_Created);
	}

	void TearDown() override
	{
		game_clock::SetTurn(0);
		Locator::entitiesRegistry::reset();
		openblack::test::ResetWorldSystems();
	}

	static ecs::Registry& Reg() { return Locator::entitiesRegistry::value(); }
};
} // namespace

TEST_F(AnimalAgeTest, AYoungAnimalGrowsUp)
{
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto a = registry.Create();
	const auto b = registry.Create();
	// a herd of two with room for a third: the breeding tests pass but for the age
	auto& herd = registry.Assign<Flock>(flock);
	herd.members = {a, b};
	herd.maxMembers = 3;
	// made aged 12: Living::SetAge, birth turn = turn - 12 x 1500; the component keeps the 12 it was made with
	auto& animal = registry.Assign<Animal>(a, AnimalInfo::Cow, 12u, flock, true);
	auto& brain = registry.Assign<AnimalBrain>(a);
	brain.birthTurn = static_cast<int32_t>(k_Created) - 12 * 1500;
	auto& transform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context ctx {a, animal, brain, transform, info};

	// 1499 turns on: (12 x 1500 + 1499) / 1500 = 12, still young for the three readers
	game_clock::SetTurn(k_Created + 1499);
	EXPECT_EQ(ai::AgeOf(brain), 12u);
	EXPECT_TRUE(ai::IsChild(ctx));
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 0);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 0);

	// 1500 turns on: 13 = grownUpAge, an adult for the three readers
	game_clock::SetTurn(k_Created + 1500);
	EXPECT_EQ(ai::AgeOf(brain), 13u);
	EXPECT_FALSE(ai::IsChild(ctx));
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 1);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 2);
	// the stored copy is not what they read
	EXPECT_EQ(animal.age, 12u);
}

TEST_F(AnimalAgeTest, TheNeedsCountUpOneATurnToTheirKindsMost)
{
	// Each turn the hunger, sleep and breeding counters grow by one up to the kind's info.dat value and stay there; the
	// breeding counter only for an adult in a flock of two or more with room for one more (docs/wiki/beings/animals.md#turn)
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto a = registry.Create();
	const auto b = registry.Create();
	auto& herd = registry.Assign<Flock>(flock);
	herd.members = {a, b};
	herd.maxMembers = 3;
	auto& animal = registry.Assign<Animal>(a, AnimalInfo::Cow, 13u, flock, true);
	auto& brain = registry.Assign<AnimalBrain>(a);
	brain.birthTurn = static_cast<int32_t>(k_Created) - 13 * 1500;
	brain.hunger = 48;
	brain.sleep = 999;
	brain.breed = 2998;
	auto& transform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context ctx {a, animal, brain, transform, info};

	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.hunger, 49);
	EXPECT_EQ(brain.sleep, 1000);
	EXPECT_EQ(brain.breed, 2999);
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.hunger, 50);
	EXPECT_EQ(brain.sleep, 1000);
	EXPECT_EQ(brain.breed, 3000);
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.hunger, 50);
	EXPECT_EQ(brain.sleep, 1000);
	EXPECT_EQ(brain.breed, 3000);

	// A full herd, or one on its own, does not count towards breeding
	brain.breed = 10;
	herd.maxMembers = 2;
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 10);
	herd.maxMembers = 3;
	herd.members = {a};
	ai::ProcessNeeds(ctx);
	EXPECT_EQ(brain.breed, 10);

	// A kind without a need never counts it: every bird's sleep is 0 in info.dat
	GAnimalInfo bird = info;
	bird.sleep = 0;
	brain.sleep = 0;
	ai::Context birdCtx {a, animal, brain, transform, bird};
	ai::ProcessNeeds(birdCtx);
	EXPECT_EQ(brain.sleep, 0);
}

TEST_F(AnimalAgeTest, AnAdultGrazerCountsTowardsBreedingAsItReactsToItsNeedsAloneOrInAFullHerd)
{
	// A grazer reacting to its needs adds one to its breeding counter whenever it is an adult, with no herd, a herd of
	// one or a full herd; at its kind's need the counter starts again, and only a herd with room gives birth
	// (docs/wiki/beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto a = registry.Create();
	const auto b = registry.Create();
	auto& herd = registry.Assign<Flock>(flock);
	auto& animal = registry.Assign<Animal>(a, AnimalInfo::Cow, 13u);
	auto& brain = registry.Assign<AnimalBrain>(a);
	brain.birthTurn = static_cast<int32_t>(k_Created) - 13 * 1500;
	auto& transform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context ctx {a, animal, brain, transform, info};
	const auto decide = static_cast<uint8_t>(ecs::animal_ai::AnimalState::DecideWhatToDo);
	const auto givesBirth = static_cast<uint8_t>(ecs::animal_ai::AnimalState::GivesBirth);

	// no herd: it counts, and at the need starts again without a birth
	brain.breed = 2998;
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 2999);
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 0);
	EXPECT_EQ(brain.topState, decide);

	// a full herd of two: it counts, and at the need starts again without a birth
	animal.flock = flock;
	herd.members = {a, b};
	herd.maxMembers = 2;
	brain.breed = 10;
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 11);
	brain.breed = 2999;
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.breed, 0);
	EXPECT_EQ(brain.topState, decide);

	// the last of its herd, with room: it counts up to the need and gives birth
	herd.members = {a};
	brain.breed = 2999;
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Started);
	EXPECT_EQ(brain.breed, 0);
	EXPECT_EQ(brain.topState, givesBirth);
}

TEST_F(AnimalAgeTest, TheLeaderCountsItsTurnsAsLeader)
{
	// The flock's leader (its first member) adds one to the flock's turns each turn it counts its needs; the others
	// do not
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto flock = registry.Create();
	const auto a = registry.Create();
	const auto b = registry.Create();
	auto& herd = registry.Assign<Flock>(flock);
	herd.members = {a, b};
	herd.maxMembers = 2;
	auto& leader = registry.Assign<Animal>(a, AnimalInfo::Cow, 0u, flock, true);
	auto& follower = registry.Assign<Animal>(b, AnimalInfo::Cow, 0u, flock, true);
	auto& leaderBrain = registry.Assign<AnimalBrain>(a);
	auto& followerBrain = registry.Assign<AnimalBrain>(b);
	auto& leaderTransform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	auto& followerTransform = registry.Assign<Transform>(b, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context leaderCtx {a, leader, leaderBrain, leaderTransform, info};
	ai::Context followerCtx {b, follower, followerBrain, followerTransform, info};

	ai::ProcessNeeds(followerCtx);
	EXPECT_EQ(herd.leaderTurns, 0u);
	ai::ProcessNeeds(leaderCtx);
	ai::ProcessNeeds(leaderCtx);
	EXPECT_EQ(herd.leaderTurns, 2u);
}

TEST_F(AnimalAgeTest, ATiredGrazerGoesToSleepAtItsSleepCellByDayOrNight)
{
	// Sleep is a counter with no day or night: with its sleep counter at its kind's 1000 and not hungry, a grazer that
	// has a sleep cell goes to it (SEEK_SLEEP), at noon as at midnight; one turn short, or with no sleep cell, it does
	// not (docs/wiki/beings/animals.md#herbivores-sheep-tortoise-cow-horse-pig)
	const test::RestoreService<Locator::infoConstants> restoreInfo;
	Locator::infoConstants::reset(std::make_unique<InfoConstants>().release());
	const auto info = CowInfo();
	auto& registry = Reg();
	const auto a = registry.Create();
	auto& animal = registry.Assign<Animal>(a, AnimalInfo::Cow, 0u);
	auto& brain = registry.Assign<AnimalBrain>(a);
	auto& transform = registry.Assign<Transform>(a, glm::vec3(0.0f), glm::mat3(1.0f), glm::vec3(1.0f));
	ai::Context ctx {a, animal, brain, transform, info};
	// a calf, so that breeding does not come first
	brain.birthTurn = static_cast<int32_t>(k_Created);
	const auto decide = static_cast<uint8_t>(ecs::animal_ai::AnimalState::DecideWhatToDo);
	const auto seekSleep = static_cast<uint8_t>(ecs::animal_ai::AnimalState::SeekSleep);

	brain.sleepCell = {10, 12};
	brain.sleep = 999;
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.topState, decide);

	for (const float hours : {12.0f, 23.5f})
	{
		Locator::animalSystem::value().SetVisualTime(hours);
		brain.topState = decide;
		brain.finalState = decide;
		brain.sleep = 1000;
		EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Started) << hours;
		EXPECT_EQ(brain.topState, seekSleep) << hours;
	}

	brain.topState = decide;
	brain.finalState = decide;
	brain.sleepCell = {0, 0};
	EXPECT_EQ(ai::CowReactToAnimalNeeds(ctx), ai::k_Nothing);
	EXPECT_EQ(brain.topState, decide);
}
