/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDeeds.h"

using namespace openblack;
using namespace openblack::creature_watching;

TEST(CreatureDeeds, WoodToABuildingThatGoesUpIsAlwaysTheBuildingSiteDeed)
{
	for (const auto receiver : {Receiver::WorshipSite, Receiver::Workshop, Receiver::Other})
	{
		EXPECT_EQ(GiftDeed({.receiver = receiver, .goingUp = true, .resource = ResourceType::Wood}),
		          Deed::PutWoodInBuildingSite);
	}
	// Food to a building that goes up is not
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Other, .goingUp = true, .resource = ResourceType::Food}).has_value());
	// Food to a worship site that goes up is still the worship site's deed
	EXPECT_EQ(GiftDeed({.receiver = Receiver::WorshipSite, .goingUp = true, .resource = ResourceType::Food}),
	          Deed::PutFoodInWorshipSite);
}

TEST(CreatureDeeds, WorshipSitesTakeFoodAndWorkshopsWood)
{
	EXPECT_EQ(GiftDeed({.receiver = Receiver::WorshipSite, .resource = ResourceType::Food}), Deed::PutFoodInWorshipSite);
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::WorshipSite, .resource = ResourceType::Wood}).has_value());
	EXPECT_EQ(GiftDeed({.receiver = Receiver::Workshop, .resource = ResourceType::Wood}), Deed::PutWoodInWorkshop);
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Workshop, .resource = ResourceType::Food}).has_value());
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Other, .resource = ResourceType::Food}).has_value());
	EXPECT_FALSE(GiftDeed({.receiver = Receiver::Other, .resource = ResourceType::Wood}).has_value());
}

TEST(CreatureDeeds, DisciplesFollowTheSeaDeedByTheirType)
{
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::None), Deed::ThrowInTheSea);
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Farmer), Deed::MakeDiscipleFarmer);
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Craftsman), Deed::MakeDiscipleCraftsman);
	// From the trader on, each type shows the deed of the next (vanilla bug)
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Trader), Deed::MakeDiscipleChangeHouse);
	EXPECT_EQ(DiscipleDeed(VillagerDisciple::Worship), Deed::TakeObjectHome);
}

TEST(CreatureDeeds, ALandingNearTheTempleIsTakenHomeOrStolen)
{
	const Landing home {.player = PlayerNames::PLAYER_ONE, .nearOwnTemple = true};
	EXPECT_EQ(LandingDeed(home), Deed::TakeObjectHome);
	Landing own = home;
	own.thingsTown = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(LandingDeed(own), Deed::TakeObjectHome);
	Landing stolen = home;
	stolen.thingsTown = PlayerNames::PLAYER_TWO;
	EXPECT_EQ(LandingDeed(stolen), Deed::StealObjectAndPutByCitadel);
	// Away from the temple, only a thing of another player's town put by the player's own town is a deed
	stolen.nearOwnTemple = false;
	EXPECT_FALSE(LandingDeed(stolen).has_value());
	stolen.nearestTown = PlayerNames::PLAYER_ONE;
	EXPECT_EQ(LandingDeed(stolen), Deed::StealObjectAndPutInTown);
	const Landing nothing {.player = PlayerNames::PLAYER_ONE, .nearestTown = PlayerNames::PLAYER_ONE};
	EXPECT_FALSE(LandingDeed(nothing).has_value());
}
