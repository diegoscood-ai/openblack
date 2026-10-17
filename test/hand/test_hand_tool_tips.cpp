/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the hand's tooltips submit over each kind of object, as the original's hand states do

#include <gtest/gtest.h>

#include "ECS/Systems/HandToolTipRules.h"

using namespace openblack;
using namespace openblack::ecs::systems;

TEST(HandToolTips, OnlyABuiltStoragePitShowsItsStore)
{
	// a pit being built shows nothing of its own, and the hand goes on to the tap or the land
	EXPECT_FALSE(hand_tooltips::StoragePitToolTip(false, 100, 200).has_value());

	const auto tip = hand_tooltips::StoragePitToolTip(true, 100, 200);
	ASSERT_TRUE(tip.has_value());
	EXPECT_EQ(tip->text, 0xEF9u);
	EXPECT_EQ(tip->action, -1);
	EXPECT_EQ(tip->align, 0u);
	EXPECT_FALSE(tip->force);
	// the food first, then the wood
	EXPECT_EQ(tip->numbers.count, 2u);
	EXPECT_EQ(tip->numbers.values[0], 100.0);
	EXPECT_EQ(tip->numbers.values[1], 200.0);
}

TEST(HandToolTips, StoragePitFoodGoesThroughAFloatAndTheWoodDoesNot)
{
	// 2^24 + 1 is the first whole number a float cannot hold: the food rounds, the wood stays exact
	constexpr uint32_t k_Odd = (1u << 24) + 1;
	const auto tip = hand_tooltips::StoragePitToolTip(true, k_Odd, k_Odd);
	ASSERT_TRUE(tip.has_value());
	EXPECT_EQ(tip->numbers.values[0], 16777216.0);
	EXPECT_EQ(tip->numbers.values[1], 16777217.0);
}

TEST(HandToolTips, ATownDesireFlagShowsTheNeedInItsOwnWords)
{
	const auto food = hand_tooltips::TownDesireFlagToolTip(TownDesireInfo::ForFood, 0.92f);
	ASSERT_TRUE(food.has_value());
	EXPECT_EQ(food->text, 0xE88u);
	EXPECT_EQ(food->action, -1);
	EXPECT_FALSE(food->force);
	ASSERT_TRUE(food->numbers.text.has_value());
	EXPECT_EQ(*food->numbers.text, 0xECEu);
	EXPECT_EQ(food->numbers.count, 1u);
	EXPECT_NEAR(food->numbers.values[0], 92.0, 1e-4);

	const auto children = hand_tooltips::TownDesireFlagToolTip(TownDesireInfo::ForChildren, 0.5f);
	ASSERT_TRUE(children.has_value());
	EXPECT_EQ(*children->numbers.text, 0xED6u);
}

TEST(HandToolTips, ATownDesireFlagsNeedIsHeldToAHundredAndADesireWithoutWordsShowsNone)
{
	const auto wood = hand_tooltips::TownDesireFlagToolTip(TownDesireInfo::ForWood, 1.7f);
	ASSERT_TRUE(wood.has_value());
	EXPECT_EQ(wood->numbers.values[0], 100.0);
	EXPECT_FALSE(hand_tooltips::TownDesireFlagToolTip(TownDesireInfo::ForPlaytime, 0.5f).has_value());
}

TEST(HandToolTips, TownCentreShowsBelieversThenPopulationWithNoButton)
{
	const auto tips = hand_tooltips::TownToolTips(250.0f, 12, 20);
	EXPECT_EQ(tips[0].text, 0xECCu);
	EXPECT_EQ(tips[0].action, -1);
	EXPECT_FALSE(tips[0].force);
	EXPECT_EQ(tips[0].numbers.values[0], 250.0);
	EXPECT_FALSE(tips[0].numbers.text.has_value());
	EXPECT_EQ(tips[1].text, 0xEFCu);
	EXPECT_EQ(tips[1].action, -1);
	EXPECT_FALSE(tips[1].force);
	EXPECT_EQ(tips[1].numbers.values[0], 12.0);
	ASSERT_TRUE(tips[1].numbers.outOf.has_value());
	EXPECT_EQ(*tips[1].numbers.outOf, 20);
}

TEST(HandToolTips, TownCentreBehindShowsTheBelieversNeeded)
{
	const auto tips = hand_tooltips::TownToolTips(-40.0f, 0, 0);
	EXPECT_EQ(tips[0].text, 0xECCu);
	EXPECT_EQ(tips[0].numbers.values[0], 40.0);
	ASSERT_TRUE(tips[0].numbers.text.has_value());
	EXPECT_EQ(*tips[0].numbers.text, 0xEE0u);
}

TEST(HandToolTips, CreatureInteractHasNoButton)
{
	const auto tip = hand_tooltips::CreatureInteractToolTip();
	EXPECT_EQ(tip.text, 0xE85u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_FALSE(tip.force);
}

TEST(HandToolTips, SeedOutOfTheInfluenceShowsTheLand)
{
	EXPECT_EQ(hand_tooltips::SeedToolTip(false, true, true, true, true, MagicType::Fireball).shows, hand_tooltips::Shows::Land);
	EXPECT_EQ(hand_tooltips::SeedToolTip(false, true, false, false, true, MagicType::Fireball).shows,
	          hand_tooltips::Shows::Land);
}

TEST(HandToolTips, NoSeedShowsNothing)
{
	EXPECT_EQ(hand_tooltips::SeedToolTip(true, false, false, false, true, MagicType::Fireball).shows,
	          hand_tooltips::Shows::Nothing);
}

TEST(HandToolTips, SeedOverAnObjectCastsOnItOrShowsTheLand)
{
	const auto valid = hand_tooltips::SeedToolTip(true, true, true, true, false, MagicType::StormWindRain);
	EXPECT_EQ(valid.shows, hand_tooltips::Shows::Own);
	EXPECT_EQ(valid.tip.text, 0xE81u); // the storms' text is only over the land
	EXPECT_EQ(valid.tip.action, 2);
	EXPECT_EQ(valid.tip.align, 0u);
	EXPECT_FALSE(valid.tip.force);
	// an object the seed cannot be cast on does not fall back to the land under it
	EXPECT_EQ(hand_tooltips::SeedToolTip(true, true, true, false, true, MagicType::Fireball).shows, hand_tooltips::Shows::Land);
}

TEST(HandToolTips, SeedOverTheLandShowsTheSizedMiraclesOwnText)
{
	for (const auto magic : {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado, MagicType::Shield,
	                         MagicType::PhysicalShield})
	{
		const auto choice = hand_tooltips::SeedToolTip(true, true, false, false, true, magic);
		EXPECT_EQ(choice.shows, hand_tooltips::Shows::Own);
		EXPECT_EQ(choice.tip.text, 0xEF1u);
		EXPECT_EQ(choice.tip.action, 2);
	}
	for (const auto magic : {MagicType::None, MagicType::Food, MagicType::FoodPowerUpOne, MagicType::Wood, MagicType::Fireball})
	{
		const auto choice = hand_tooltips::SeedToolTip(true, true, false, false, true, magic);
		EXPECT_EQ(choice.shows, hand_tooltips::Shows::Own);
		EXPECT_EQ(choice.tip.text, 0xE81u);
	}
	EXPECT_EQ(hand_tooltips::SeedToolTip(true, true, false, false, false, MagicType::Tornado).shows,
	          hand_tooltips::Shows::Land);
}

TEST(HandToolTips, OneOffOrbShowsItsOwnTextForcedWithNoButton)
{
	const auto tip = hand_tooltips::OrbPickUpToolTip(0xE90);
	EXPECT_EQ(tip.text, 0xE90u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_TRUE(tip.force);
	EXPECT_EQ(tip.numbers.count, 0u);
}

TEST(HandToolTips, SpellIconShowsItsSeedsTooltipForcedWithNoButton)
{
	const auto tip = hand_tooltips::SpellIconToolTip(0xE94);
	EXPECT_EQ(tip.text, 0xE94u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_TRUE(tip.force);
	// a seed with no tooltip submits a text the help system drops, so the icon shows nothing, not even the land's
	EXPECT_LT(hand_tooltips::SpellIconToolTip(0).text, openblack::help::tooltips::k_First);
}

TEST(HandToolTips, OnlyABuiltWorkshopShowsItsRawText)
{
	// a workshop being built goes on to the entrance, the storage pit and the tap
	EXPECT_FALSE(hand_tooltips::WorkshopToolTip(false).has_value());

	const auto tip = hand_tooltips::WorkshopToolTip(true);
	ASSERT_TRUE(tip.has_value());
	EXPECT_EQ(tip->text, 0xEF3u);
	EXPECT_EQ(tip->action, -1);
	EXPECT_EQ(tip->align, 0u);
	EXPECT_FALSE(tip->force);
	// no numbers: the text is shown as written
	EXPECT_EQ(tip->numbers.count, 0u);
	EXPECT_FALSE(tip->numbers.outOf.has_value());
	EXPECT_FALSE(tip->numbers.text.has_value());
}

TEST(HandToolTips, VillageTotemShowsItsTownCentresTexts)
{
	const auto centre = static_cast<entt::entity>(4);
	const auto totem = static_cast<entt::entity>(9);
	const auto other = static_cast<entt::entity>(12);
	// a town centre: its own town
	EXPECT_EQ(hand_tooltips::TownCentreForToolTips(centre, true, false, entt::null), centre);
	// a totem: its town centre's town, the same texts
	EXPECT_EQ(hand_tooltips::TownCentreForToolTips(totem, false, true, centre), centre);
	// a totem with no town centre: a town object with no town, so no text and no other test
	const auto none = hand_tooltips::TownCentreForToolTips(totem, false, true, entt::null);
	ASSERT_TRUE(none.has_value());
	EXPECT_TRUE(*none == entt::null);
	// anything else goes on to the other tests
	EXPECT_FALSE(hand_tooltips::TownCentreForToolTips(other, false, false, entt::null).has_value());
}

TEST(HandToolTips, ObjectHandStateTakesTheOriginalsOrder)
{
	// an interactable object in the influence that is neither lockable nor placeable
	const hand_tooltips::ObjectFacts plain {.interactable = true, .inInfluence = true, .mustBeInInfluence = true};
	EXPECT_EQ(hand_tooltips::ObjectHandState(plain), 18);

	// not interactable: 3 before anything else
	auto facts = plain;
	facts.interactable = false;
	facts.inInfluence = false;
	facts.validForLockedSelect = true;
	facts.validForPlaceInHand = true;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 3);

	// out of the influence: 18 when interaction needs it, before the locked select and the hand
	facts = plain;
	facts.inInfluence = false;
	facts.validForLockedSelect = true;
	facts.validForPlaceInHand = true;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 18);
	// an object that does not need the influence goes on
	facts.mustBeInInfluence = false;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 13);
	facts.validForLockedSelect = false;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 9);

	// valid for a locked select: 13, before the hand
	facts = plain;
	facts.validForLockedSelect = true;
	facts.validForPlaceInHand = true;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 13);

	// valid to place in the hand: 9, unless it cannot be picked up
	facts = plain;
	facts.validForPlaceInHand = true;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 9);
	facts.cannotBePickedUp = true;
	EXPECT_EQ(hand_tooltips::ObjectHandState(facts), 18);
}
