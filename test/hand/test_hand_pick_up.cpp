/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the hand takes as it picks something up or scoops from a source (hand_pick_up), who of the villagers it can reach
// (villager::ReachableWhenAvailable) and which rocks it can lift (physics::objects::RockFitsInHand)

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Villager/VillagerHome.h"
#include "Enums.h"
#include "Hand/HandPickUpRules.h"
#include "Physics/ObjectRules.h"

using namespace openblack;
namespace pick = openblack::hand_pick_up;

namespace
{
/// The place-in-hand's checks, answered from a list and recorded in the order they are asked
struct Checks
{
	std::vector<pick::PlaceInHandCheck> refused;
	std::vector<pick::PlaceInHandCheck> asked;

	bool operator()(pick::PlaceInHandCheck check)
	{
		asked.push_back(check);
		for (const auto no : refused)
		{
			if (no == check)
			{
				return false;
			}
		}
		return true;
	}
};
} // namespace

TEST(HandPickUp, ThePlaceInHandTakesAThingOnlyWhenEveryCheckPassesAskedInTheGamesOrder)
{
	using enum pick::PlaceInHandCheck;
	Checks all;
	EXPECT_TRUE(pick::PlaceInHandTakes(all));
	EXPECT_EQ(all.asked, (std::vector {Interactable, HandEmpty, Valid, MayBePickedUp, NotCarried}));
	// a thing in a hand already, made unpickable by a script or carried by a tornado is refused, and no later check
	// is asked
	for (const auto refused : {Interactable, HandEmpty, Valid, MayBePickedUp, NotCarried})
	{
		Checks one {.refused = {refused}};
		EXPECT_FALSE(pick::PlaceInHandTakes(one));
		EXPECT_EQ(one.asked.back(), refused);
	}
	Checks held {.refused = {HandEmpty}};
	EXPECT_FALSE(pick::PlaceInHandTakes(held));
	EXPECT_EQ(held.asked, (std::vector {Interactable, HandEmpty}));
}

TEST(HandPickUp, AGrabThatHasWaitedTakesOnlyInTheInfluenceWhatNoScriptForbids)
{
	EXPECT_TRUE(pick::GrabTakes(true, false));
	EXPECT_FALSE(pick::GrabTakes(false, false));
	EXPECT_FALSE(pick::GrabTakes(true, true));
	EXPECT_FALSE(pick::GrabTakes(false, true));
}

TEST(HandPickUp, AFirstHandfulIsWhatTheHandsPileTakesAtFirstOrWhatTheSourceHas)
{
	// the hand's pots take 25 at first
	EXPECT_EQ(pick::FirstHandful(1000, 25), 25u);
	EXPECT_EQ(pick::FirstHandful(25, 25), 25u);
	EXPECT_EQ(pick::FirstHandful(7, 25), 7u);
	// nothing from an empty pile or store
	EXPECT_EQ(pick::FirstHandful(0, 25), 0u);
	// wood fills the hand's wood, anything else the hand's food
	EXPECT_EQ(pick::HandPileFor(ResourceType::Wood), PotInfo::HandWood);
	EXPECT_EQ(pick::HandPileFor(ResourceType::Food), PotInfo::HandFood);
}

TEST(HandPickUp, AFieldIsScoopedOnlyWhileItHasGrownAndHasFood)
{
	EXPECT_TRUE(pick::FieldValidForLockedSelect(0.1f, 1.5f));
	EXPECT_FALSE(pick::FieldValidForLockedSelect(0.0f, 100.0f));
	EXPECT_FALSE(pick::FieldValidForLockedSelect(10.0f, 1.0f));
	EXPECT_FALSE(pick::FieldValidForLockedSelect(-1.0f, 100.0f));
}

TEST(HandPickUp, AFieldsFirstHandfulIsTwentyFiveOrItsFoodHalvedWhenRipe)
{
	EXPECT_FLOAT_EQ(ecs::components::Field::k_TakenWithHand, 25.0f);
	const float k_Taken = ecs::components::Field::k_TakenWithHand;
	EXPECT_EQ(pick::FieldFirstHandful(k_Taken, 400.0f, false), 25);
	EXPECT_EQ(pick::FieldFirstHandful(k_Taken, 400.0f, true), 12);
	// what it has, cut towards zero, then halved
	EXPECT_EQ(pick::FieldFirstHandful(k_Taken, 13.9f, false), 13);
	EXPECT_EQ(pick::FieldFirstHandful(k_Taken, 13.9f, true), 6);
	EXPECT_EQ(pick::FieldFirstHandful(k_Taken, 0.5f, false), 0);
}

// A field put straight into the hand, when the scooping refuses it, gives its hand amount (25 in info.dat), cut towards
// zero, whatever food it has and ripe or not
TEST(HandPickUp, AFieldTakenStraightGivesItsHandAmountWhateverFoodItHas)
{
	EXPECT_EQ(pick::FieldStraightHandful(ecs::components::Field::k_TakenWithHand), 25);
	EXPECT_EQ(pick::FieldStraightHandful(24.9f), 24);
	EXPECT_EQ(pick::FieldStraightHandful(0.5f), 0);
}

// A fish farm put straight into the hand gives the hand food's food value (20 in info.dat), cut towards zero, while the
// farm's own food value is above 0; nothing otherwise
TEST(HandPickUp, AFishFarmTakenStraightGivesTheHandFoodsFoodValue)
{
	constexpr float k_HandFoodValue = 20.0f;
	EXPECT_EQ(pick::FishFarmStraightHandful(k_HandFoodValue, ecs::components::FishFarm::k_FoodValue), std::optional(20));
	EXPECT_EQ(pick::FishFarmStraightHandful(20.7f, 1.0f), std::optional(20));
	EXPECT_EQ(pick::FishFarmStraightHandful(k_HandFoodValue, 0.0f), std::nullopt);
	EXPECT_EQ(pick::FishFarmStraightHandful(k_HandFoodValue, -1.0f), std::nullopt);
}

TEST(HandPickUp, AFieldsScoopRampsOverSixtyTurnsHalvedWhenRipeAndKeepsUnderTwentyThousand)
{
	// 8 + 62 t^2, t = turns / 60, at most 1
	EXPECT_EQ(pick::FieldTurnScoop(0, 1000.0f, 0, false).amount, 8);
	EXPECT_FLOAT_EQ(pick::FieldTurnScoop(0, 1000.0f, 0, false).t, 0.0f);
	EXPECT_EQ(pick::FieldTurnScoop(30, 1000.0f, 0, false).amount, 23);
	EXPECT_FLOAT_EQ(pick::FieldTurnScoop(30, 1000.0f, 0, false).t, 0.5f);
	EXPECT_EQ(pick::FieldTurnScoop(60, 1000.0f, 0, false).amount, 70);
	EXPECT_EQ(pick::FieldTurnScoop(600, 1000.0f, 0, false).amount, 70);
	EXPECT_FLOAT_EQ(pick::FieldTurnScoop(600, 1000.0f, 0, false).t, 1.0f);
	// the ramp halved too when the crop is ripe
	EXPECT_EQ(pick::FieldTurnScoop(30, 1000.0f, 0, true).amount, 11);
	EXPECT_EQ(pick::FieldTurnScoop(60, 1000.0f, 0, true).amount, 35);
	// no more than the field has, cut towards zero
	EXPECT_EQ(pick::FieldTurnScoop(60, 10.5f, 0, false).amount, 10);
	// no more than the room under 20000 in the hand
	EXPECT_EQ(pick::FieldTurnScoop(60, 1000.0f, 19990, false).amount, 10);
	EXPECT_EQ(pick::FieldTurnScoop(60, 1000.0f, 20000, false).amount, 0);
}

TEST(HandPickUp, APilesScoopTakesNoMoreThanItHasNorThanTheHandfulsRoom)
{
	// a handful holds up to 20000
	EXPECT_EQ(pick::PileScoopTaken(70, 1000, 20000, 0), 70u);
	EXPECT_EQ(pick::PileScoopTaken(70, 30, 20000, 0), 30u);
	EXPECT_EQ(pick::PileScoopTaken(70, 1000, 20000, 19980), 20u);
	EXPECT_EQ(pick::PileScoopTaken(70, 1000, 20000, 20000), 0u);
	EXPECT_EQ(pick::PileScoopTaken(70, 1000, 20000, 25000), 0u);
	EXPECT_EQ(pick::PileScoopTaken(70, 0, 20000, 0), 0u);
}

TEST(HandPickUp, TheScoopingSoundRisesFromSixtyToTwoHundredAndFortyPercent)
{
	EXPECT_EQ(pick::PickUpSoundSample(true), 98);
	EXPECT_EQ(pick::PickUpSoundSample(false), 44);
	// 60 + 180 t^2, cut towards zero
	EXPECT_EQ(pick::PickUpSoundPitch(0.0f), 60);
	EXPECT_EQ(pick::PickUpSoundPitch(0.25f), 105);
	EXPECT_EQ(pick::PickUpSoundPitch(1.0f), 240);
	EXPECT_EQ(pick::PickUpSoundPitch(0.999f), 239);
	// truncated, not rounded
	EXPECT_EQ(pick::PickUpSoundPitch(0.1f), 78);
	EXPECT_EQ(pick::PickUpSoundPitch(0.004f), 60);
}

TEST(HandPickUp, EachKindOfThingHasItsOwnHold)
{
	using pick::HoldClass;
	// on the palm by default: three quarters of its height, no lowering
	auto hold = pick::HoldParametersOf(HoldClass::Object, MobileStaticInfo::None, 2.0f, 5.0f);
	EXPECT_EQ(hold.type, HoldType::Above);
	EXPECT_FLOAT_EQ(hold.radius, 1.5f);
	EXPECT_FLOAT_EQ(hold.lowering, 0.0f);
	// a tree: a fifth of its radius, 0.1
	hold = pick::HoldParametersOf(HoldClass::Tree, MobileStaticInfo::None, 10.0f, 5.0f);
	EXPECT_EQ(hold.type, HoldType::Tree);
	EXPECT_FLOAT_EQ(hold.radius, 1.0f);
	EXPECT_FLOAT_EQ(hold.lowering, 0.1f);
	// a lump of poo on the palm: three quarters of its height, a little above the hand point
	hold = pick::HoldParametersOf(HoldClass::Poo, MobileStaticInfo::None, 2.0f, 5.0f);
	EXPECT_EQ(hold.type, HoldType::Above);
	EXPECT_FLOAT_EQ(hold.radius, 1.5f);
	EXPECT_FLOAT_EQ(hold.lowering, -0.3f);
	// a mobile object, a pot (a handful too) or a one-shot orb from the side: its radius, 0.7
	hold = pick::HoldParametersOf(HoldClass::SideObject, MobileStaticInfo::None, 2.0f, 0.8f);
	EXPECT_EQ(hold.type, HoldType::Side);
	EXPECT_FLOAT_EQ(hold.radius, 0.8f);
	EXPECT_FLOAT_EQ(hold.lowering, 0.7f);
	// a villager or an animal by the villager grip: its radius, 0.65
	hold = pick::HoldParametersOf(HoldClass::Living, MobileStaticInfo::None, 2.0f, 0.4f);
	EXPECT_EQ(hold.type, HoldType::Villager);
	EXPECT_FLOAT_EQ(hold.radius, 0.4f);
	EXPECT_FLOAT_EQ(hold.lowering, 0.65f);
}

TEST(HandPickUp, SomeStaticsAreHeldFromTheSideTheRestOnThePalm)
{
	using pick::HoldClass;
	for (const auto heavy :
	     {MobileStaticInfo::GateTotemApe, MobileStaticInfo::GateTotemBlank, MobileStaticInfo::GateTotemCow,
	      MobileStaticInfo::GateTotemTiger, MobileStaticInfo::WeepingStone, MobileStaticInfo::WeepingStoneReward})
	{
		const auto hold = pick::HoldParametersOf(HoldClass::MobileStatic, heavy, 6.0f, 2.0f);
		EXPECT_EQ(hold.type, HoldType::Side);
		EXPECT_FLOAT_EQ(hold.radius, 2.0f);
		EXPECT_FLOAT_EQ(hold.lowering, 0.7f);
	}
	auto hold = pick::HoldParametersOf(HoldClass::MobileStatic, MobileStaticInfo::SingingStone_1, 6.0f, 2.0f);
	EXPECT_EQ(hold.type, HoldType::Side);
	EXPECT_FLOAT_EQ(hold.lowering, 0.4f);
	// the cuddly toy and the skittle from the side, with no lowering
	for (const auto toy : {MobileStaticInfo::ToyCuddly, MobileStaticInfo::ToySkittle})
	{
		hold = pick::HoldParametersOf(HoldClass::MobileStatic, toy, 1.0f, 0.5f);
		EXPECT_EQ(hold.type, HoldType::Side);
		EXPECT_FLOAT_EQ(hold.radius, 0.5f);
		EXPECT_FLOAT_EQ(hold.lowering, 0.0f);
	}
	// any other static on the palm
	hold = pick::HoldParametersOf(HoldClass::MobileStatic, MobileStaticInfo::CeltFenceShort, 2.0f, 3.0f);
	EXPECT_EQ(hold.type, HoldType::Above);
	EXPECT_FLOAT_EQ(hold.radius, 1.5f);
	EXPECT_FLOAT_EQ(hold.lowering, 0.0f);
}

TEST(HandPickUp, AVillagerCanBeReachedUnlessAtHomeHeldOrHiding)
{
	EXPECT_TRUE(ecs::villager::ReachableWhenAvailable(false, false, VillagerStates::MoveToPos));
	EXPECT_FALSE(ecs::villager::ReachableWhenAvailable(true, false, VillagerStates::MoveToPos));
	EXPECT_FALSE(ecs::villager::ReachableWhenAvailable(false, true, VillagerStates::MoveToPos));
	EXPECT_FALSE(ecs::villager::ReachableWhenAvailable(false, false, VillagerStates::GoAndHideInNearbyBuilding));
}

TEST(HandPickUp, RocksWiderThanThreePointSixCantBeLifted)
{
	EXPECT_TRUE(physics::objects::RockFitsInHand(1.0f));
	EXPECT_TRUE(physics::objects::RockFitsInHand(3.6f));
	EXPECT_FALSE(physics::objects::RockFitsInHand(3.61f));
	EXPECT_FALSE(physics::objects::RockFitsInHand(10.0f));
}
