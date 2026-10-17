/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <bit>
#include <limits>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureEating.h"

using namespace openblack;
using creature_eating::Eaten;
using creature_eating::MealEnd;

namespace
{
constexpr float k_NaN = std::numeric_limits<float>::quiet_NaN();
} // namespace

TEST(CreatureEatingCanBeEaten, AnObjectIsEatenWhenItCanBePickedUp)
{
	EXPECT_TRUE(creature_eating::CanBeEaten({.as = Eaten::AnObject, .mayPickUp = true}));
	EXPECT_FALSE(creature_eating::CanBeEaten({.as = Eaten::AnObject, .mayPickUp = false}));
	// a toy object is still eaten: only a mobile static asks whether it is a toy
	EXPECT_TRUE(creature_eating::CanBeEaten({.as = Eaten::AnObject, .mayPickUp = true, .isToy = true}));
}

TEST(CreatureEatingCanBeEaten, AMobileStaticIsEatenUnlessItIsAToy)
{
	EXPECT_TRUE(creature_eating::CanBeEaten({.as = Eaten::AMobileStatic}));
	EXPECT_FALSE(creature_eating::CanBeEaten({.as = Eaten::AMobileStatic, .isToy = true}));
	// whether it can be picked up is not asked
	EXPECT_TRUE(creature_eating::CanBeEaten({.as = Eaten::AMobileStatic, .mayPickUp = false}));
}

TEST(CreatureEatingCanBeEaten, NothingElseIsEaten)
{
	EXPECT_FALSE(creature_eating::CanBeEaten({.as = Eaten::Never, .mayPickUp = true}));
}

TEST(CreatureEatingPortion, AmountIsAThousandTimesTheDrawnSizeCutTowardsZero)
{
	EXPECT_EQ(creature_eating::PortionAmount(1.0f, 5000), 1000u);
	EXPECT_EQ(creature_eating::PortionAmount(0.37f, 5000), 370u);
	EXPECT_EQ(creature_eating::PortionAmount(0.9999f, 5000), 999u);
	// the product is a float before the cut: 0.051f is a little under 0.051, and its exact product under 51
	EXPECT_EQ(creature_eating::PortionAmount(0.051f, 5000), 51u);
}

TEST(CreatureEatingPortion, AmountIsNoMoreThanTheSourceHolds)
{
	EXPECT_EQ(creature_eating::PortionAmount(1.0f, 600), 600u);
	EXPECT_EQ(creature_eating::PortionAmount(0.6f, 600), 600u);
	EXPECT_EQ(creature_eating::PortionAmount(0.6f, 601), 600u);
	EXPECT_EQ(creature_eating::PortionAmount(1.0f, 0), 0u);
	// a size not a number converts to the integer indefinite, compared unsigned: the source's amount
	EXPECT_EQ(creature_eating::PortionAmount(k_NaN, 2500), 2500u);
	EXPECT_EQ(creature_eating::PortionAmount(k_NaN, 0xFFFFFFFFu), 0x80000000u);
}

TEST(CreatureEatingPortion, ItGoesToWithinOneAndAFifthTheTwoRadiiOfAPile)
{
	EXPECT_EQ(creature_eating::PileApproach(2.0f, 3.0f), 6.0f);
	// the sum is a float before the share: 0.1 + 0.2 rounds up to 0x3E99999A, times 1.2 to 0x3EB851EC
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_eating::PileApproach(0.1f, 0.2f)), 0x3EB851ECu);
}

TEST(CreatureEatingPortion, ScaleIsThreeAndThreeQuartersTheDrawnSize)
{
	EXPECT_EQ(creature_eating::PortionScale(1.2f), 4.5f);
	EXPECT_EQ(creature_eating::PortionScale(1.0f), 3.75f);
}

TEST(CreatureEatingPortion, PointIsNineTenthsAndAHalfOfTheRangeTowardsTheSource)
{
	const glm::vec2 creature {100.0f, 200.0f};
	// far: 50 m away along (3, 4)
	const auto far = creature_eating::PortionPoint(creature, creature + glm::vec2 {30.0f, 40.0f}, 10.0f);
	EXPECT_NEAR(glm::distance(far, creature), 9.5f, 1e-4f);
	EXPECT_NEAR(far.x, 105.7f, 1e-4f);
	EXPECT_NEAR(far.y, 207.6f, 1e-4f);
	// near: a source 1 m away still gives a point 9.5 m away, beyond it
	const auto near = creature_eating::PortionPoint(creature, creature + glm::vec2 {0.0f, -1.0f}, 10.0f);
	EXPECT_EQ(near.x, 100.0f);
	EXPECT_NEAR(near.y, 190.5f, 1e-4f);
}

TEST(CreatureEatingPortion, PointIsTheCreaturesWhenTheSourceIsAtItsPlace)
{
	const glm::vec2 creature {12.0f, -7.0f};
	const auto point = creature_eating::PortionPoint(creature, creature, 10.0f);
	EXPECT_EQ(point, creature);
}

TEST(CreatureEatingPortion, ADifferenceNotANumberCountsAsZero)
{
	const glm::vec2 creature {12.0f, -7.0f};
	// across not a number and along zero: no scaling, the difference is added as it is
	auto point = creature_eating::PortionPoint(creature, {k_NaN, -7.0f}, 10.0f);
	EXPECT_TRUE(std::isnan(point.x));
	EXPECT_EQ(point.y, -7.0f);
	// along it not zero: scaled, and the length is not a number
	point = creature_eating::PortionPoint(creature, {k_NaN, 0.0f}, 10.0f);
	EXPECT_TRUE(std::isnan(point.x));
	EXPECT_TRUE(std::isnan(point.y));
}

TEST(CreatureEatingMeal, LessThanFiveIsNotFoodAndFiveIsEaten)
{
	EXPECT_EQ(creature_eating::JudgeMeal(4.99f, false, false, false), MealEnd::NotFood);
	EXPECT_EQ(creature_eating::JudgeMeal(std::nextafter(5.0f, 0.0f), false, false, false), MealEnd::NotFood);
	EXPECT_EQ(creature_eating::JudgeMeal(5.0f, false, false, false), MealEnd::Eaten);
	EXPECT_EQ(creature_eating::JudgeMeal(2000.0f, false, false, false), MealEnd::Eaten);
	EXPECT_EQ(creature_eating::JudgeMeal(k_NaN, false, false, false), MealEnd::NotFood);
}

TEST(CreatureEatingMeal, APoisonedThingIsSpatOutWhateverItIsWorth)
{
	EXPECT_EQ(creature_eating::JudgeMeal(100.0f, false, false, true), MealEnd::Poisoned);
	EXPECT_EQ(creature_eating::JudgeMeal(3.0f, false, false, true), MealEnd::Poisoned);
	EXPECT_EQ(creature_eating::JudgeMeal(k_NaN, false, false, true), MealEnd::Poisoned);
}

TEST(CreatureEatingMeal, AMushroomOrPileFoodIsAlwaysEaten)
{
	EXPECT_EQ(creature_eating::JudgeMeal(0.0f, true, false, false), MealEnd::Eaten);
	EXPECT_EQ(creature_eating::JudgeMeal(0.0f, false, true, false), MealEnd::Eaten);
	EXPECT_EQ(creature_eating::JudgeMeal(0.0f, true, false, true), MealEnd::Eaten);
	EXPECT_EQ(creature_eating::JudgeMeal(k_NaN, false, true, true), MealEnd::Eaten);
}

TEST(CreatureEatingMeal, HungerGoesDownByTheMealOverTheSizeCappedAtTheLargestMeal)
{
	// a size of 1 counts as 0.8: 400 / (0.8 x 1000)
	EXPECT_FLOAT_EQ(creature_eating::HungerAfterMeal(1.0f, 400.0f, 1.0f, 1000.0f), 0.5f);
	EXPECT_FLOAT_EQ(creature_eating::HungerAfterMeal(1.0f, 400.0f, 0.8f, 1000.0f), 0.5f);
	// a smaller size takes more off: 400 / (0.5 x 1000)
	EXPECT_FLOAT_EQ(creature_eating::HungerAfterMeal(1.0f, 400.0f, 0.5f, 1000.0f), 0.2f);
}

TEST(CreatureEatingMeal, HungerIsKeptWithinZeroAndOne)
{
	EXPECT_EQ(creature_eating::HungerAfterMeal(0.3f, 2000.0f, 1.0f, 1000.0f), 0.0f);
	EXPECT_EQ(creature_eating::HungerAfterMeal(0.9f, -400.0f, 1.0f, 1000.0f), 1.0f);
	EXPECT_EQ(creature_eating::HungerAfterMeal(0.5f, 400.0f, 1.0f, 1000.0f), 0.0f);
	EXPECT_EQ(creature_eating::HungerAfterMeal(1.0f, 0.0f, 1.0f, 1000.0f), 1.0f);
	// no food to energy: the whole hunger goes
	EXPECT_EQ(creature_eating::HungerAfterMeal(0.7f, 400.0f, 1.0f, 0.0f), 0.0f);
}

TEST(CreatureEatingMeal, HungerNotANumberGivesZero)
{
	EXPECT_EQ(creature_eating::HungerAfterMeal(k_NaN, 400.0f, 1.0f, 1000.0f), 0.0f);
	EXPECT_EQ(creature_eating::HungerAfterMeal(1.0f, k_NaN, 1.0f, 1000.0f), 0.0f);
	// a size not a number is kept as the meal size, not the cap
	EXPECT_EQ(creature_eating::HungerAfterMeal(1.0f, 400.0f, k_NaN, 1000.0f), 0.0f);
	// nothing over nothing
	EXPECT_EQ(creature_eating::HungerAfterMeal(1.0f, 0.0f, 1.0f, 0.0f), 0.0f);
}

TEST(CreatureEatingPile, OnlyMagicFoodIsAFoodPileToEatFrom)
{
	EXPECT_FALSE(creature_eating::IsFoodPileOutsideStoragePit(false, std::nullopt, 100.0f));
	EXPECT_FALSE(creature_eating::IsFoodPileOutsideStoragePit(false, 10.0f, 100.0f));
}

TEST(CreatureEatingPile, AMagicFoodPileWithNoStoragePitIsOutside)
{
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, std::nullopt, 0.0f));
}

TEST(CreatureEatingPile, AMagicFoodPileAtThePitsReachIsOutside)
{
	const float reach = 10.0f * creature_eating::k_PitReachPerRadius;
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, reach));
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, std::nextafter(reach, 100.0f)));
	EXPECT_FALSE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, std::nextafter(reach, 0.0f)));
	EXPECT_FALSE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, 0.0f));
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, 25.0f));
}

TEST(CreatureEatingPile, ADistanceOrRadiusNotANumberIsOutside)
{
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, 10.0f, k_NaN));
	EXPECT_TRUE(creature_eating::IsFoodPileOutsideStoragePit(true, k_NaN, 1.0f));
}

TEST(CreatureEatingPit, APitNoWiderThanFifteenTimesTheSizeWithFoodHasFood)
{
	const float limit = 1.2f * creature_eating::k_PitRadiusPerSize;
	EXPECT_TRUE(creature_eating::StoragePitHasFood(limit, 1.2f, true, false, 1));
	EXPECT_TRUE(creature_eating::StoragePitHasFood(std::nextafter(limit, 0.0f), 1.2f, true, false, 2500));
	EXPECT_FALSE(creature_eating::StoragePitHasFood(std::nextafter(limit, 100.0f), 1.2f, true, false, 2500));
	// a radius not a number passes
	EXPECT_TRUE(creature_eating::StoragePitHasFood(k_NaN, 1.2f, true, false, 2500));
}

TEST(CreatureEatingPit, APitWithoutAPileOrWithAnEmptyOrMarkedOneHasNone)
{
	EXPECT_FALSE(creature_eating::StoragePitHasFood(5.0f, 1.0f, false, false, 2500));
	EXPECT_FALSE(creature_eating::StoragePitHasFood(5.0f, 1.0f, true, true, 2500));
	EXPECT_FALSE(creature_eating::StoragePitHasFood(5.0f, 1.0f, true, false, 0));
}

TEST(CreatureEatingPit, TheOriginalsEdgesForTheWidthAndThePile)
{
	// 1.2 x 15 rounds to exactly 18: 18 passes, the next float up does not
	EXPECT_EQ(std::bit_cast<uint32_t>(1.2f * creature_eating::k_PitRadiusPerSize), 0x41900000u);
	EXPECT_TRUE(creature_eating::StoragePitHasFood(std::bit_cast<float>(0x41900000u), 1.2f, true, false, 1));
	EXPECT_FALSE(creature_eating::StoragePitHasFood(std::bit_cast<float>(0x41900001u), 1.2f, true, false, 2500));
	EXPECT_TRUE(creature_eating::StoragePitHasFood(std::bit_cast<float>(0x418FFFFFu), 1.2f, true, false, 2500));
	// a size not a number lets any width pass
	EXPECT_TRUE(creature_eating::StoragePitHasFood(100.0f, k_NaN, true, false, 2500));
	// any amount but none is food, the top bit too
	EXPECT_TRUE(creature_eating::StoragePitHasFood(5.0f, 1.2f, true, false, 0x80000000u));
}

TEST(CreatureEatingField, AFieldHasFoodWhenItsCropHasGrownAndHoldsFood)
{
	EXPECT_TRUE(creature_eating::FieldHasFood(1.0f, 1.0f));
	EXPECT_TRUE(creature_eating::FieldHasFood(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()));
	// the smallest amounts above nothing count
	EXPECT_TRUE(creature_eating::FieldHasFood(std::bit_cast<float>(1u), std::bit_cast<float>(1u)));
}

TEST(CreatureEatingField, NothingLessOrNotANumberIsNoFood)
{
	EXPECT_FALSE(creature_eating::FieldHasFood(0.0f, 1.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(1.0f, 0.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(-0.0f, 1.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(1.0f, -0.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(-1.0f, 1.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(1.0f, -1.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(k_NaN, 1.0f));
	EXPECT_FALSE(creature_eating::FieldHasFood(1.0f, k_NaN));
}

TEST(CreatureEatingTree, ATreeFifteenTimesTheDrawnSizeTallIsTallEnough)
{
	EXPECT_TRUE(creature_eating::TreeTallEnough(15.0f, 1.0f));
	EXPECT_FALSE(creature_eating::TreeTallEnough(std::nextafter(15.0f, 0.0f), 1.0f));
	EXPECT_TRUE(creature_eating::TreeTallEnough(30.0f, 1.0f));
	// 1.1 x 15 rounds to exactly 16.5: the floats on either side of it fall either way
	EXPECT_EQ(std::bit_cast<uint32_t>(1.1f * creature_eating::k_TreeHeightPerSize), 0x41840000u);
	EXPECT_TRUE(creature_eating::TreeTallEnough(16.5f, 1.1f));
	EXPECT_TRUE(creature_eating::TreeTallEnough(std::bit_cast<float>(0x41840001u), 1.1f));
	EXPECT_FALSE(creature_eating::TreeTallEnough(std::bit_cast<float>(0x4183FFFFu), 1.1f));
	// 0.37 x 15 rounds up to the float nearest 5.55, which is then just tall enough
	EXPECT_TRUE(creature_eating::TreeTallEnough(5.55f, 0.37f));
	EXPECT_FALSE(creature_eating::TreeTallEnough(std::nextafter(5.55f, 0.0f), 0.37f));
	// a creature of no size eats from any tree
	EXPECT_TRUE(creature_eating::TreeTallEnough(0.0f, 0.0f));
	EXPECT_TRUE(creature_eating::TreeTallEnough(-0.0f, 0.0f));
	EXPECT_TRUE(creature_eating::TreeTallEnough(std::numeric_limits<float>::infinity(), 1.0f));
}

TEST(CreatureEatingTree, AHeightOrSizeNotANumberIsNotTallEnough)
{
	EXPECT_FALSE(creature_eating::TreeTallEnough(k_NaN, 1.0f));
	EXPECT_FALSE(creature_eating::TreeTallEnough(100.0f, k_NaN));
}
