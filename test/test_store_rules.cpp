/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>

#include <gtest/gtest.h>

#include "ECS/StoreRules.h"

using namespace openblack::ecs;

TEST(StoreRules, ATreesWoodScalesOnceWithItsSizeAndWithItsLife)
{
	EXPECT_EQ(store_rules::TreeWood(1.0f, 1.0f, 100, 2.0f, 1.0f), 200u);
	EXPECT_EQ(store_rules::TreeWood(0.5f, 1.0f, 100, 2.0f, 1.0f), 100u);
	// A forest miracle's tree and the land's balance multiply it, and it is truncated
	EXPECT_EQ(store_rules::TreeWood(1.0f, 1.5f, 100, 1.0f, 0.999f), 149u);
}

TEST(StoreRules, ADeadTreesWoodIgnoresItsLife)
{
	EXPECT_EQ(store_rules::DeadTreeWood(2.0f, 100, 1.0f), 200u);
}

TEST(StoreRules, AFencesWoodGrowsWithTheCubeOfItsSize)
{
	EXPECT_EQ(store_rules::FenceWood(1.0f, 10, 2.0f), 80u);
	EXPECT_EQ(store_rules::FenceWood(0.5f, 10, 2.0f), 40u);
	EXPECT_EQ(store_rules::FenceWood(1.0f, 25, 1.0f), 25u);
	EXPECT_EQ(store_rules::FenceWood(1.0f, 25, 0.9f), 18u); // 18.225
	EXPECT_EQ(store_rules::FenceWood(0.5f, 25, 1.0f), 12u); // 12.5
	EXPECT_EQ(store_rules::FenceWood(0.0f, 25, 1.0f), 0u);
}

TEST(StoreRules, OnlyMeatAndVegetableAnimalsAreFood)
{
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 1), 300u);
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 2), 300u);
	EXPECT_EQ(store_rules::AnimalFood(300.0f, 4), 0u);
	EXPECT_EQ(store_rules::AnimalFood(1200.0f, 3), 1200u);
	EXPECT_EQ(store_rules::AnimalFood(1200.0f, 0), 0u);
	EXPECT_EQ(store_rules::AnimalFood(1200.0f, 5), 1200u);
	EXPECT_EQ(store_rules::AnimalFood(99.9f, 1), 99u); // truncated
}

TEST(StoreRules, AMushroomIsWorthItsFoodValue)
{
	EXPECT_EQ(store_rules::MushroomFood(99.0f), 99u);
	EXPECT_EQ(store_rules::MushroomFood(150.9f), 150u); // truncated
	EXPECT_EQ(store_rules::MushroomFood(0.0f), 0u);
}

TEST(StoreRules, ARocksWoodIsItsScaleTimesItsWoodValue)
{
	// the original's numbers for a rock worth 200 wood at these scales
	EXPECT_EQ(store_rules::RockWood(1.0f, 200), 200u);
	EXPECT_EQ(store_rules::RockWood(0.7935f, 200), 158u);
	EXPECT_EQ(store_rules::RockWood(0.5f, 200), 100u);
	EXPECT_EQ(store_rules::RockWood(1.37f, 200), 274u);
	EXPECT_EQ(store_rules::RockWood(2.9f, 200), 580u);
	EXPECT_EQ(store_rules::RockWood(0.123f, 200), 24u);
	EXPECT_EQ(store_rules::RockWood(0.62963f, 200), 125u);
}

TEST(StoreRules, ARockKeepsTheShareOfItsWoodLeftAsItsScale)
{
	// the original's numbers: 121 of 200 left is scale 0.605, and a rock of scale 0.7935 at 0.75 above the ground
	// comes down to 0.57183367; 1 of 200 is 0.005, and 1.5 above the ground at scale 1.37 comes down to 0.0054744524
	EXPECT_EQ(std::bit_cast<uint32_t>(store_rules::RockScaleLeft(121, 200)), 0x3F1AE148u);
	EXPECT_EQ(std::bit_cast<uint32_t>(store_rules::RockAltitudeLeft(0.75f, store_rules::RockScaleLeft(121, 200), 0.7935f)),
	          0x3F1263B1u);
	EXPECT_EQ(std::bit_cast<uint32_t>(store_rules::RockScaleLeft(1, 200)), 0x3BA3D70Au);
	EXPECT_EQ(std::bit_cast<uint32_t>(store_rules::RockAltitudeLeft(1.5f, store_rules::RockScaleLeft(1, 200), 1.37f)),
	          0x3BB36309u);
	EXPECT_EQ(store_rules::RockAltitudeLeft(0.0f, 0.75f, 1.0f), 0.0f);
}

TEST(StoreRules, GivingBackWhatWasTakenLatelyCountsForLess)
{
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(std::nullopt, 100, 50), 1.0f);
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(75, 100, 50), 0.125f);
	EXPECT_FLOAT_EQ(store_rules::LastTakenModifier(0, 100, 50), 1.0f);
}

TEST(StoreRules, APileGivesOnlyWhatIsWithinItsStoresMaximum)
{
	EXPECT_EQ(store_rules::AmountOverMaximum(true, 600, 100), 100);
	EXPECT_EQ(store_rules::AmountOverMaximum(false, 600, 100), 500);
	EXPECT_EQ(store_rules::AskedOfPile(50, 100), 0u);
	EXPECT_EQ(store_rules::AskedOfPile(150, 100), 50u);
	EXPECT_EQ(store_rules::AskedOfPile(150, -10), 150u);
}

TEST(StoreRules, AForestCountsItsWoodWithoutTruncating)
{
	// A small tree worth less than one wood still counts for its forest
	EXPECT_FLOAT_EQ(store_rules::TreeWoodValue(0.5f, 1.0f, 1u, 0.5f, 1.0f), 0.25f);
	EXPECT_EQ(store_rules::TreeWood(0.5f, 1.0f, 1u, 0.5f, 1.0f), 0u);
}

TEST(StoreRules, AGiftToATownCountsForTheDropInItsDesire)
{
	// The town's multipliers in the game's data: a giver who does not own the town counts in full, and a quarter of the
	// drop goes to the town's belief
	constexpr float k_NonOwner = 1.0f;
	constexpr float k_ToTown = 0.25f;
	// the desire falls from 0.75 to 0.25: the drop to the giver's alignment, a quarter of it to the town's belief
	const auto gift = store_rules::GiftToTown(0.75f, 0.25f, 1.0f, k_NonOwner, k_ToTown);
	EXPECT_EQ(gift.alignmentChange, 0.5f);
	EXPECT_EQ(gift.belief, 0.125f);
	// a town that wanted it less drops less, so the same gift counts for less
	const auto less = store_rules::GiftToTown(0.375f, 0.125f, 1.0f, k_NonOwner, k_ToTown);
	EXPECT_EQ(less.alignmentChange, 0.25f);
	EXPECT_EQ(less.belief, 0.0625f);
	// a town that does not want it at all: nothing
	const auto none = store_rules::GiftToTown(0.0f, 0.0f, 1.0f, k_NonOwner, k_ToTown);
	EXPECT_EQ(none.alignmentChange, 0.0f);
	EXPECT_EQ(none.belief, 0.0f);
	// taken from the town lately (half the turns to forget it: an eighth), both count by the modifier
	const auto lately =
	    store_rules::GiftToTown(0.75f, 0.25f, store_rules::LastTakenModifier(75, 100, 50), k_NonOwner, k_ToTown);
	EXPECT_EQ(lately.alignmentChange, 0.0625f);
	EXPECT_EQ(lately.belief, 0.015625f);
	// a share other than one for the giver changes only the town's belief
	const auto share = store_rules::GiftToTown(0.75f, 0.25f, 1.0f, 0.5f, k_ToTown);
	EXPECT_EQ(share.alignmentChange, 0.5f);
	EXPECT_EQ(share.belief, 0.0625f);
}
