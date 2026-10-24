/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>

#include <gtest/gtest.h>

#include "Creature/CreaturePickUpRules.h"

using namespace openblack;
using creature_pick_up::Check;
using creature_pick_up::CreatureMayPickUp;

namespace
{
/// A thing well within reach and light enough, that may be picked up
Check Liftable()
{
	return {.thingRadius = 1.0f,
	        .creatureRadius = 2.0f,
	        .reach = 10.0f,
	        .thingWeight = 50.0f,
	        .creatureWeight = 1000.0f,
	        .thingAllows = true};
}
} // namespace

TEST(CreaturePickUpRules, ALightThingInReachIsPickedUp)
{
	EXPECT_TRUE(CreatureMayPickUp(Liftable()));
}

TEST(CreaturePickUpRules, AComputerPlayersCreatureLeavesTheScriptsThingsAlone)
{
	auto check = Liftable();
	check.computerPlayer = true;
	check.thingInScript = true;
	EXPECT_FALSE(CreatureMayPickUp(check));
	// unless a script controls the creature too
	check.creatureScripted = true;
	EXPECT_TRUE(CreatureMayPickUp(check));
	// a player's creature may take them
	check = Liftable();
	check.thingInScript = true;
	EXPECT_TRUE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, ItLeavesItsOwnWorshippersAndWhatItsLeashIsTiedTo)
{
	auto check = Liftable();
	check.ownWorshipper = true;
	EXPECT_FALSE(CreatureMayPickUp(check));
	check = Liftable();
	check.leashTiedTo = true;
	EXPECT_FALSE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, AThingOffTheMapOnlyWhenInItsHand)
{
	auto check = Liftable();
	check.inMap = false;
	EXPECT_FALSE(CreatureMayPickUp(check));
	check.inThisHand = true;
	EXPECT_TRUE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, BothRadiiWithinTheReachEqualPassing)
{
	auto check = Liftable();
	check.reach = 3.0f;
	EXPECT_TRUE(CreatureMayPickUp(check));
	check.reach = 2.9999998f;
	EXPECT_FALSE(CreatureMayPickUp(check));
	check.reach = std::numeric_limits<float>::quiet_NaN();
	EXPECT_TRUE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, AtMostEightTenthsOfItsWeightEqualPassing)
{
	auto check = Liftable();
	check.creatureWeight = 100.0f;
	check.thingWeight = 80.0f;
	EXPECT_TRUE(CreatureMayPickUp(check));
	check.thingWeight = 80.00001f;
	EXPECT_FALSE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, TheThingHasTheLastSay)
{
	auto check = Liftable();
	check.thingAllows = false;
	EXPECT_FALSE(CreatureMayPickUp(check));
}

TEST(CreaturePickUpRules, AThingWeighsItsScaleCubedTimesItsKinds)
{
	EXPECT_FLOAT_EQ(creature_pick_up::ThingWeight(1.0f, 30.0f), 30.0f);
	EXPECT_FLOAT_EQ(creature_pick_up::ThingWeight(2.0f, 30.0f), 240.0f);
	EXPECT_FLOAT_EQ(creature_pick_up::ThingWeight(0.5f, 8.0f), 1.0f);
}

TEST(CreaturePickUpRules, OnlyAWorshipperOfItsOwnPlayerIsLeft)
{
	using creature_pick_up::IsOwnWorshipper;
	EXPECT_TRUE(IsOwnWorshipper(true, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	// another player's worshipper, a villager doing something else, and one with no town are not
	EXPECT_FALSE(IsOwnWorshipper(true, PlayerNames::PLAYER_TWO, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(IsOwnWorshipper(false, PlayerNames::PLAYER_ONE, PlayerNames::PLAYER_ONE));
	EXPECT_FALSE(IsOwnWorshipper(true, std::nullopt, PlayerNames::PLAYER_ONE));
}

TEST(CreaturePickUpRules, AVillagerInsideItsHomeSaysNo)
{
	using creature_pick_up::VillagerAllows;
	EXPECT_TRUE(VillagerAllows(false, false));
	EXPECT_FALSE(VillagerAllows(true, false));
	EXPECT_FALSE(VillagerAllows(false, true));
	EXPECT_FALSE(VillagerAllows(true, true));
}

TEST(CreaturePickUpRules, APileAsksOnlyItsFlagAndWhetherItsKindMayMove)
{
	using creature_pick_up::PileAllows;
	EXPECT_TRUE(PileAllows(false, true));
	EXPECT_FALSE(PileAllows(true, true));
	EXPECT_FALSE(PileAllows(false, false));
	EXPECT_FALSE(PileAllows(true, false));
}

TEST(CreaturePickUpRules, AVillagerWeighsItsScaleCubedTimesItsInfo)
{
	// every villager row of the game's info weighs 82.5
	EXPECT_FLOAT_EQ(creature_pick_up::ThingWeight(1.0f, 82.5f), 82.5f);
	EXPECT_FLOAT_EQ(creature_pick_up::ThingWeight(0.8f, 82.5f), 0.8f * 0.8f * 0.8f * 82.5f);
}
