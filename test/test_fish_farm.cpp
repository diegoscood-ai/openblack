/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Fish farms and their shoals: the stock's takers, the fishermen's rules, the shoal's centre, opacity, wandering point
// and scare, a fish's step and the hand's test for a shown fish. Fake draws and fake farm sides only, no game data.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/geometric.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/Billboard.h"
#include "3D/FrameAnim.h"
#include "3D/MapCoords.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishFarms.h"
#include "ECS/FishShoals.h"
#include "ECS/Registry.h"
#include "ECS/Villager/VillagerFisherman.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "support/RestoreService.h"
#include "support/VillagerFakes.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace fish_farms = openblack::ecs::fish_farms;
namespace villager = openblack::ecs::villager;
namespace billboard = openblack::graphics::billboard;
namespace frame_anim = openblack::graphics::frame_anim;

namespace
{
/// Draws the middle of every range, and remembers the ranges asked for
struct MiddleRandom
{
	std::vector<std::pair<float, float>>* asked;
	float operator()(float low, float high) const
	{
		asked->emplace_back(low, high);
		return (low + high) * 0.5f;
	}
};

/// The game's one fish farm info: a full stock of 1400, meat, a fish back every 16 turns, 4 fishermen
GFishFarmInfo GameFishFarmInfo()
{
	GFishFarmInfo info {};
	info.foodValue = 1400.0f;
	info.foodType = FoodType::Meat;
	info.numGameTurnsAfterWhichFoodIsIncreased = 16;
	info.maxNoFishermanPerFishFarm = 4;
	return info;
}

Fish FishAt(glm::vec3 position)
{
	Fish fish {};
	fish.position = position;
	fish.halfSize = 1.0f;
	return fish;
}

/// A registry and the game's fish farm info of their own, put back when the test ends
class FishFarmStock: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		auto info = std::make_unique<InfoConstants>();
		info->fishFarm = GameFishFarmInfo();
		Locator::infoConstants::reset(info.release());
	}

	static entt::entity MakeFarm(float food)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<FishFarm>(entity).food = food;
		return entity;
	}

	static FishFarm& Farm(entt::entity entity) { return Locator::entitiesRegistry::value().Get<FishFarm>(entity); }

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::infoConstants> _restoreInfo;
};

/// A town's fish farms through a fake farm side: each farm's score is the game's rule for its fisherman count
class FishFarmFinder: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		test::FakeVillagerFishFarms ops;
		ops.townFishFarms = [this](entt::entity) { return order; };
		ops.score = [this](entt::entity farm) { return fish_farms::ScoreFor(fishermen.at(farm), 4); };
		Locator::villagerFishFarms::emplace<test::FakeVillagerFishFarms>(ops);
	}

	static entt::entity Thing(glm::vec2 at)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<Transform>(entity, glm::vec3(at.x, 0.0f, at.y), glm::mat3(1.0f), glm::vec3(1.0f));
		return entity;
	}

	entt::entity AddFarm(float distance, size_t count)
	{
		const auto farm = Thing({100.0f + distance, 100.0f});
		fishermen[farm] = count;
		order.push_back(farm);
		return farm;
	}

	test::RestoreService<Locator::entitiesRegistry> _restoreRegistry;
	test::RestoreService<Locator::villagerFishFarms> _restoreFarms;
	std::vector<entt::entity> order;
	std::map<entt::entity, size_t> fishermen;
};
} // namespace

// ---- the farm's stock and the hand ----------------------------------------------------------------------------------

TEST_F(FishFarmStock, TakingFishTakesNoMoreThanItHas)
{
	const auto farm = MakeFarm(10.5f);
	EXPECT_EQ(fish_farms::RemoveFood(farm, 4), 4);
	EXPECT_FLOAT_EQ(Farm(farm).food, 6.5f);
	EXPECT_EQ(fish_farms::RemoveFood(farm, 9), 6);
	EXPECT_FLOAT_EQ(Farm(farm).food, 0.0f);
}

TEST(FishFarm, AsManyFishShowAsTheFarmIsFull)
{
	FishFarm farm;
	EXPECT_EQ(farm.VisibleFish(), 0u);
	farm.shoal = FishShoal {};
	EXPECT_EQ(farm.VisibleFish(), 15u);
	farm.shoal->fullness = 0.5f;
	EXPECT_EQ(farm.VisibleFish(), 7u);
	farm.shoal->fullness = 1.0f / 16.0f;
	EXPECT_EQ(farm.VisibleFish(), 0u);
}

TEST(FishFarm, AFishComesBackOnEverySixteenthTurnUpToFull)
{
	const auto info = GameFishFarmInfo();
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(10.0f, 16, info), 11.0f);
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(10.0f, 17, info), 10.0f);
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(1400.0f, 16, info), 1400.0f);
	// kept within its stock on every turn, not only on those it grows
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(-3.0f, 17, info), 0.0f);
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(2000.0f, 17, info), 1400.0f);
	// the growth and the stock are its info's: every 4 turns up to 100
	auto other = info;
	other.foodValue = 100.0f;
	other.numGameTurnsAfterWhichFoodIsIncreased = 4;
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(10.0f, 8, other), 11.0f);
	EXPECT_FLOAT_EQ(ecs::FishFarmFoodAfterTurn(100.0f, 8, other), 100.0f);
}

TEST(FishFarm, AFarmHoldsItsInfosFoodOnlyForAFoodItIs)
{
	auto info = GameFishFarmInfo();
	// any food, or its own type: the info's food value
	EXPECT_FLOAT_EQ(fish_farms::FoodValue(info, FoodType::Any), 1400.0f);
	EXPECT_FLOAT_EQ(fish_farms::FoodValue(info, FoodType::Meat), 1400.0f);
	// another type, not any food: nought
	EXPECT_FLOAT_EQ(fish_farms::FoodValue(info, FoodType::Vegetable), fish_farms::k_NoFood);
	// a farm of no food (or only grazing) holds none, whatever is asked
	info.foodType = FoodType::None;
	EXPECT_FLOAT_EQ(fish_farms::FoodValue(info, FoodType::Any), 0.0f);
	info.foodType = FoodType::Graze;
	EXPECT_FLOAT_EQ(fish_farms::FoodValue(info, FoodType::Graze), 0.0f);
}

TEST_F(FishFarmStock, TheShownFishFollowTheStockOnTheFarmsTurn)
{
	const auto farm = MakeFarm(1400.0f);
	Farm(farm).shoal = FishShoal {};
	// a scoop takes half: the fish still show as full until the farm's process
	EXPECT_EQ(fish_farms::RemoveFood(farm, 700), 700);
	EXPECT_EQ(Farm(farm).VisibleFish(), 15u);
	ecs::ProcessFishFarmsTurn(17);
	EXPECT_FLOAT_EQ(Farm(farm).shoal->fullness, 0.5f);
	EXPECT_EQ(Farm(farm).VisibleFish(), 7u);
}

TEST(FishFarm, TheHandsFirstHandfulGoesByAFullFarmNotTheFishInIt)
{
	EXPECT_EQ(fish_farms::FirstHandful(25, 100.0f), 25u);
	EXPECT_EQ(fish_farms::FirstHandful(250, 100.0f), 100u);
}

TEST(FishFarm, AScoopWantsItsRampWithinTheHandfulsRoom)
{
	EXPECT_EQ(fish_farms::ScoopWanted(40, 100.0f, 0, 20000), 40u);
	EXPECT_EQ(fish_farms::ScoopWanted(400, 100.0f, 0, 20000), 100u);
	EXPECT_EQ(fish_farms::ScoopWanted(40, 100.0f, 90, 100), 10u);
	EXPECT_EQ(fish_farms::ScoopWanted(40, 100.0f, 120, 100), 0u);
}

// ---- the fishermen --------------------------------------------------------------------------------------------------

TEST(FishFarm, ATownWantsAFarmFishedOnlyWhileItHasNoFisherman)
{
	EXPECT_EQ(fish_farms::ScoreFor(0, 4), 1);
	EXPECT_EQ(fish_farms::ScoreFor(1, 4), 0);
	EXPECT_EQ(fish_farms::ScoreFor(4, 4), 0);
	EXPECT_EQ(fish_farms::ScoreFor(9, 4), 0);
	// the nearer of two farms without a fisherman scores more; one with a fisherman nothing
	EXPECT_GT(villager::FishFarmScore(1, 100.0f), villager::FishFarmScore(1, 200.0f));
	EXPECT_EQ(villager::FishFarmScore(0, 50.0f), 0.0f);
}

TEST_F(FishFarmFinder, ATownSendsAFishermanToTheNearestFarmWithoutOne)
{
	const auto town = Thing({0.0f, 0.0f});
	const auto me = Thing({100.0f, 100.0f});
	AddFarm(50.0f, 1);
	AddFarm(200.0f, 0);
	const auto nearest = AddFarm(100.0f, 0);
	AddFarm(100.0f, 0);
	float score = 0.0f;
	EXPECT_EQ(villager::FindBestFishFarm(town, me, score), nearest);
	EXPECT_GT(score, 0.0f);
}

TEST_F(FishFarmFinder, NoFarmThatHasItsFishermanHoweverNear)
{
	const auto town = Thing({0.0f, 0.0f});
	const auto me = Thing({100.0f, 100.0f});
	AddFarm(5.0f, 1);
	AddFarm(50.0f, 2);
	float score = -1.0f;
	EXPECT_EQ(villager::FindBestFishFarm(town, me, score), entt::entity(entt::null));
	EXPECT_EQ(score, 0.0f);
}

TEST(FishFarm, AFishermansSpotIsTheFarmMovedByMetres)
{
	// 2.5 m along x and -1 m along z from the middle of cell (10, 20)
	const map_coords::MapCoords farm {.x = (10 << 16) | 0x8000, .z = (20 << 16) | 0x8000};
	const auto spot = fish_farms::FishingSpotAt(farm, 2.5f, -1.0f);
	EXPECT_EQ(spot.x, farm.x + 16384);
	// truncated towards 0 after the move: 6553.6 units back is 6554 whole ones
	EXPECT_EQ(spot.z, farm.z - 6554);
	// each step rounded to single precision, as the original: in double the same move gives 8462028
	EXPECT_EQ(fish_farms::FishingSpotAt({.x = 8470054, .z = 0}, -1.2246549f, 0.0f).x, 8462029);
}

TEST(FishFarm, AFishermanCatchesAQuarterOfWhatHeCarriesLessOutOfSeason)
{
	const float base = villager::FishCatchBase(40);
	// spring: a quarter of 40
	EXPECT_EQ(villager::FishCatch(base, 0, 40, 1.0f), 10);
	// winter: 0.6 of it, truncated
	EXPECT_EQ(villager::FishCatch(base, 3, 40, 1.0f), 6);
	// no more than his room, then by his tribe's skill
	EXPECT_EQ(villager::FishCatch(base, 0, 4, 1.0f), 4);
	EXPECT_EQ(villager::FishCatch(base, 0, 40, 1.5f), 15);
}

TEST(FishFarm, AFishermanTakesHisFoodToTheStoreOnceHisRoomIsLessThanHisCatch)
{
	// 40 carried at most: the room left after 20, 31, 40 and 30 held
	EXPECT_FALSE(fish_farms::TakesCatchToStore(20, 10.0f));
	EXPECT_TRUE(fish_farms::TakesCatchToStore(9, 10.0f));
	EXPECT_TRUE(fish_farms::TakesCatchToStore(0, 0.0f));
	EXPECT_FALSE(fish_farms::TakesCatchToStore(10, 10.0f));
}

// ---- the shoal ------------------------------------------------------------------------------------------------------

TEST(FishShoal, TheShoalLiesWhereTheSeaIsOpenOnTwoRingsRunning)
{
	// open sea everywhere east of x = 5: the first direction, east, is open on the rings at 6 and 8 m
	const auto centre = ecs::FindShoalCentre({0.0f, 0.0f}, [](glm::vec2 point) { return point.x > 5.0f; });
	ASSERT_TRUE(centre.has_value());
	EXPECT_NEAR(centre->x, 8.0f, 1e-4f);
	EXPECT_NEAR(centre->y, 0.0f, 1e-4f);
	// no open sea: no shoal
	EXPECT_FALSE(ecs::FindShoalCentre({0.0f, 0.0f}, [](glm::vec2) { return false; }).has_value());
}

TEST(FishShoal, AShoalFadesOutPastTwoHundredMetresAndIsGoneAtThreeHundred)
{
	EXPECT_EQ(ecs::ShoalAlpha(100.0f * 100.0f), 255);
	EXPECT_EQ(ecs::ShoalAlpha(48400.0f), 40);
	// past where it has faded right out the game's opacity wraps round, as its byte does
	EXPECT_EQ(ecs::ShoalAlpha(62500.0f), 194);
	EXPECT_FALSE(ecs::ShoalAlpha(301.0f * 301.0f).has_value());
}

TEST(FishShoal, AFishIsMadeAboutItsCentre)
{
	std::vector<std::pair<float, float>> asked;
	FishShoal shoal {};
	ecs::InitFishShoal(shoal, {100.0f, -2.0f, 50.0f}, MiddleRandom {&asked});
	const auto& fish = shoal.fish[0];
	EXPECT_FLOAT_EQ(fish.halfSize, 1.0f);
	EXPECT_EQ(fish.position, glm::vec3(100.0f, -2.5f, 50.0f));
	EXPECT_FLOAT_EQ(fish.speed, 1.0f);
	EXPECT_NEAR(fish.turnRate, 0.6283185f, 1e-6f);
	EXPECT_FLOAT_EQ(fish.fleeTime, 0.0f);
	EXPECT_EQ(shoal.target, shoal.centre);
	// ten draws per fish, z then y then x for its place
	ASSERT_EQ(asked.size(), 10u * FishShoal::k_FishCount);
	EXPECT_EQ(asked[4], std::make_pair(-5.0f, 5.0f));
	EXPECT_EQ(asked[5], std::make_pair(-1.0f, 0.0f));
}

TEST(FishShoal, AFishSwimsTheWayItFacesAndTurnsTowardsThePoint)
{
	auto fish = FishAt({0.0f, -1.0f, 0.0f});
	fish.speed = 1.0f;
	fish.turnRate = 1.0f;
	// the point lies to its left (towards +z): it turns that way
	ecs::StepFish(fish, {10.0f, 0.0f, 10.0f}, 0.05f);
	EXPECT_NEAR(fish.position.x, 0.05f, 1e-6f);
	EXPECT_FLOAT_EQ(fish.position.y, -1.0f);
	EXPECT_NEAR(fish.heading, 0.05f, 1e-6f);
	// its picture runs 25 frames a second for its speed, through cells 8 to 23
	EXPECT_EQ(fish.cell, 9u);
	// a long frame counts as a tenth of a second
	ecs::StepFish(fish, {10.0f, 0.0f, 10.0f}, 5.0f);
	EXPECT_NEAR(fish.heading, 0.15f, 1e-5f);
}

TEST(FishShoal, AFishsHeadingWrapsOnlyPastAHalfTurnUpwards)
{
	auto fish = FishAt({});
	fish.turnRate = 5.0f;
	// turning left past pi it comes round to the negative side
	fish.heading = 3.0f;
	ecs::StepFish(fish, {0.0f, 0.0f, -10.0f}, 0.1f);
	EXPECT_NEAR(fish.heading, 3.5f - 6.2831855f, 1e-6f);
	// turning right past -pi it is left there, until a whole turn comes off
	fish.heading = -3.0f;
	ecs::StepFish(fish, {0.0f, 0.0f, 10.0f}, 0.1f);
	EXPECT_FLOAT_EQ(fish.heading, -3.5f);
	fish.heading = -6.0f;
	ecs::StepFish(fish, {0.0f, 0.0f, -10.0f}, 0.1f);
	EXPECT_NEAR(fish.heading, -6.5f + 6.2831855f, 1e-6f);
}

TEST(FishShoal, APanickingFishDartsFourTimesAsFast)
{
	auto fish = FishAt({});
	fish.speed = 1.0f;
	fish.fleeTime = 2.0f;
	ecs::StepFish(fish, {}, 0.1f);
	EXPECT_NEAR(fish.position.x, 0.4f, 1e-6f);
}

TEST(FishShoal, APanickingFishSlowsOverItsLastSecondFromWhatIsLeft)
{
	auto fish = FishAt({});
	fish.speed = 1.0f;
	// 0.6 s left, a 0.1 s step: 0.5 left, so 1 + 3 x 0.5 times its speed
	fish.fleeTime = 0.6f;
	ecs::StepFish(fish, {}, 0.1f);
	EXPECT_NEAR(fish.position.x, 0.1f * (1.0f + 3.0f * 0.5f), 1e-6f);
	EXPECT_NEAR(fish.fleeTime, 0.5f, 1e-6f);
	// a long frame runs the fright out by all of it, though the swim is a tenth of a second
	fish.position = {};
	fish.fleeTime = 2.0f;
	ecs::StepFish(fish, {}, 5.0f);
	EXPECT_FLOAT_EQ(fish.fleeTime, 0.0f);
	EXPECT_NEAR(fish.position.x, 0.1f, 1e-6f);
}

TEST(FishShoal, AScareSendsTheNearFishAwayAndTheShoalElsewhere)
{
	std::vector<std::pair<float, float>> asked;
	FishShoal shoal {};
	shoal.timer = 10.0f;
	shoal.fish.at(0) = FishAt({3.0f, 0.0f, 0.0f});
	shoal.fish.at(1) = FishAt({30.0f, 0.0f, 0.0f});
	shoal.fish.at(2) = FishAt({1.0f, 0.0f, 0.0f});
	// two fish shown
	EXPECT_EQ(ecs::ScareShoal(shoal, 2, glm::vec3(0.0f), MiddleRandom {&asked}), 1u);
	// the near fish darts straight away; the far one and those not showing are left be
	EXPECT_FLOAT_EQ(shoal.fish.at(0).fleeTime, 2.0f);
	EXPECT_NEAR(shoal.fish.at(0).heading, 0.0f, 1e-6f);
	EXPECT_FLOAT_EQ(shoal.fish.at(1).fleeTime, 0.0f);
	EXPECT_FLOAT_EQ(shoal.fish.at(2).fleeTime, 0.0f);
	EXPECT_FLOAT_EQ(shoal.timer, 2.0f);
	// the shoal heads for a point two metres from its centre, at an angle drawn round the circle
	ASSERT_EQ(asked.size(), 1u);
	EXPECT_NEAR(glm::length(shoal.target), 2.0f, 1e-5f);
}

TEST(FishShoal, AScaredShoalHeadsTheWayItsAngleTurnsFromX)
{
	FishShoal shoal {};
	shoal.centre = {10.0f, 0.0f, 10.0f};
	// a quarter turn: two metres along +z
	ecs::ScareShoal(shoal, 0, glm::vec3(0.0f), [](float, float) { return std::numbers::pi_v<float> * 0.5f; });
	EXPECT_NEAR(shoal.target.x, 10.0f, 1e-5f);
	EXPECT_FLOAT_EQ(shoal.target.y, 0.0f);
	EXPECT_NEAR(shoal.target.z, 12.0f, 1e-5f);
}

TEST(FishShoal, TheShoalsPointMovesOnWhenItsTimeIsUp)
{
	std::vector<std::pair<float, float>> asked;
	FishShoal shoal {};
	shoal.centre = {10.0f, 0.0f, 10.0f};
	shoal.target = {10.0f, 0.0f, 16.0f};
	shoal.timer = 0.05f;
	ecs::RetargetShoal(shoal, 0.1f, MiddleRandom {&asked});
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(asked[0], std::make_pair(-7.0f, 7.0f));
	EXPECT_EQ(shoal.target, glm::vec3(10.0f, 0.0f, 10.0f));
	EXPECT_FLOAT_EQ(shoal.timer, 3.0f);
	// with time left the point stays
	ecs::RetargetShoal(shoal, 1.0f, MiddleRandom {&asked});
	EXPECT_EQ(asked.size(), 2u);
	EXPECT_FLOAT_EQ(shoal.timer, 2.0f);
}

TEST(FishShoal, AShoalsFrameMovesItsShownFishAndCountsThoseInsideItsBait)
{
	std::vector<std::pair<float, float>> asked;
	FishShoal shoal {};
	shoal.timer = 10.0f;
	shoal.fish.at(0) = FishAt({1.0f, 0.0f, 0.0f});
	shoal.fish.at(1) = FishAt({20.0f, 0.0f, 0.0f});
	shoal.fish.at(2) = FishAt({2.0f, 0.0f, 0.0f});
	for (auto& fish : shoal.fish)
	{
		fish.speed = 1.0f;
	}
	FishBait bait;
	bait.position = glm::vec3(0.0f);
	// two fish shown, no splash: one of them inside the bait's 11 units, the hidden one left alone
	const auto move = ecs::MoveShoal(shoal, 2, 0.05f, std::nullopt, &bait, MiddleRandom {&asked});
	EXPECT_EQ(move.inside, 1u);
	EXPECT_EQ(move.fled, 0u);
	EXPECT_TRUE(asked.empty());
	EXPECT_FLOAT_EQ(shoal.timer, 9.95f);
	EXPECT_NEAR(shoal.fish.at(0).position.x, 1.05f, 1e-6f);
	EXPECT_EQ(shoal.fish.at(0).cell, 9u);
	EXPECT_FLOAT_EQ(shoal.fish.at(2).position.x, 2.0f);
	// a splash near the first fish: it flees
	const auto scared = ecs::MoveShoal(shoal, 2, 0.05f, glm::vec3(0.0f), nullptr, MiddleRandom {&asked});
	EXPECT_EQ(scared.fled, 1u);
	EXPECT_EQ(scared.inside, 0u);
	EXPECT_EQ(asked.size(), 1u);
}

TEST(FishShoal, TheShoalsTimeRunsDownByTheWholeFrame)
{
	std::vector<std::pair<float, float>> asked;
	FishShoal shoal {};
	shoal.centre = {10.0f, 0.0f, 10.0f};
	shoal.target = {10.0f, 0.0f, 16.0f};
	shoal.timer = 0.3f;
	// a half-second frame: the fish swim a tenth of it, but the point's time is up
	static_cast<void>(ecs::MoveShoal(shoal, 0, 0.5f, std::nullopt, nullptr, MiddleRandom {&asked}));
	ASSERT_EQ(asked.size(), 2u);
	EXPECT_EQ(shoal.target, glm::vec3(10.0f, 0.0f, 10.0f));
	EXPECT_FLOAT_EQ(shoal.timer, 3.0f);
}

TEST(FishShoal, AShoalStillStepsItsFishOnAFrameWithoutTime)
{
	FishShoal shoal {};
	shoal.timer = 10.0f;
	shoal.fish.at(0) = FishAt({1.0f, 0.0f, 0.0f});
	// its frame wrapped after the last step, which showed cell 23
	shoal.fish.at(0).frame = 0.5f;
	shoal.fish.at(0).cell = 23;
	FishBait bait;
	bait.position = glm::vec3(0.0f);
	const auto move = ecs::MoveShoal(shoal, 1, 0.0f, std::nullopt, &bait, [](float, float) { return 0.0f; });
	EXPECT_EQ(shoal.fish.at(0).cell, 8u);
	EXPECT_EQ(move.inside, 1u);
	EXPECT_EQ(shoal.fish.at(0).position, glm::vec3(1.0f, 0.0f, 0.0f));
}

TEST(FishShoal, ABaitIsDoneOnceItsFishHaveBeenInsideForItsHoldTime)
{
	FishBait bait;
	bait.need = 1;
	bait.holdMs = 100;
	// none inside: no time held, the net still bobs
	EXPECT_FALSE(ecs::StepFishBait(bait, 0, 0.05f));
	EXPECT_EQ(bait.timerMs, 0u);
	EXPECT_NEAR(bait.net.phase, 0.1f, 1e-6f);
	// one inside for 50 ms, then 100: done, and the net closes
	bait.inside = 0;
	EXPECT_FALSE(ecs::StepFishBait(bait, 1, 0.05f));
	EXPECT_EQ(bait.timerMs, 50u);
	bait.inside = 0;
	EXPECT_TRUE(ecs::StepFishBait(bait, 1, 0.05f));
	EXPECT_TRUE(bait.done);
	EXPECT_TRUE(bait.net.closing);
	// once done it counts no more
	bait.inside = 0;
	EXPECT_FALSE(ecs::StepFishBait(bait, 1, 0.05f));
	EXPECT_EQ(bait.timerMs, 100u);
}

TEST(FishShoal, ABaitsTimeAndNetRunByTheWholeFrame)
{
	FishBait bait;
	bait.need = 1;
	// a half-second frame holds the bait its whole 500 ms at once, and the net bobs by all of it
	EXPECT_TRUE(ecs::StepFishBait(bait, 1, 0.5f));
	EXPECT_EQ(bait.timerMs, 500u);
	EXPECT_NEAR(bait.net.phase, 1.0f, 1e-6f);
}

TEST(FishShoal, TheHandFindsAShownFishWithinTwoMetres)
{
	FishShoal shoal {};
	shoal.fish.at(0) = FishAt({10.0f, -1.0f, 10.0f});
	shoal.fish.at(1) = FishAt({20.0f, -1.0f, 20.0f});
	// one fish shown
	EXPECT_TRUE(ecs::HasShownFishNear(shoal, 1, {11.0f, 0.0f, 11.0f}));
	EXPECT_FALSE(ecs::HasShownFishNear(shoal, 1, {12.0f, 0.0f, 10.0f}));
	// a fish that doesn't show isn't found
	EXPECT_FALSE(ecs::HasShownFishNear(shoal, 1, {20.0f, 0.0f, 20.0f}));
}

TEST(FishShoal, AFishLiesFlatAlongItsWayAndShowsItsFrame)
{
	// the fish's sprite: horizontal, a quarter turn
	billboard::Sprite sprite;
	sprite.position = {0.0f, -1.0f, 0.0f};
	sprite.angle = std::numbers::pi_v<float> * 0.5f;
	sprite.horizontal = true;
	const auto quad = billboard::Horizontal(sprite);
	// its picture's first column runs back from where it swims to
	EXPECT_NEAR(quad.corners[0].x, 1.0f, 1e-6f);
	EXPECT_NEAR(quad.corners[0].z, -1.0f, 1e-6f);
	EXPECT_NEAR(quad.corners[1].z, 1.0f, 1e-6f);
	EXPECT_FLOAT_EQ(quad.corners[2].y, -1.0f);
	const auto atlas = billboard::CellUv(10, 8);
	EXPECT_EQ(atlas[0], glm::vec2(0.25f, 0.125f));
	EXPECT_EQ(atlas[2], glm::vec2(0.375f, 0.25f));
	// the frame runs through cells 8..23 and wraps every 15
	float frame = 0.0f;
	EXPECT_EQ(frame_anim::FishFrame(frame, 0.05f, 1.0f), 9u);
	EXPECT_FLOAT_EQ(frame, 1.25f);
}
