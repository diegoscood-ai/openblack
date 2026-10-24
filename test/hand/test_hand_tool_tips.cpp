/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// What the hand's tooltips submit over each kind of object, as the original's hand states do

#include <cstdint>

#include <bit>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "ECS/Systems/HandToolTipRules.h"
#include "Help/InputPromptIcon.h"

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

TEST(HandToolTips, AWorshipSitesAltarShowsThePrayerPowerItsSiteStores)
{
	const auto tip = hand_tooltips::PrayerEnergyToolTip(23200.4f);
	EXPECT_EQ(tip.text, 0xEE9u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_FALSE(tip.force);
	// one number, the store as the float it is; the text prints it whole
	EXPECT_EQ(tip.numbers.count, 1u);
	EXPECT_EQ(tip.numbers.values[0], static_cast<double>(23200.4f));
	EXPECT_FALSE(tip.numbers.outOf.has_value());
	EXPECT_FALSE(tip.numbers.text.has_value());
	// an empty store still shows
	EXPECT_EQ(hand_tooltips::PrayerEnergyToolTip(0.0f).numbers.values[0], 0.0);
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

TEST(HandToolTips, AScriptHighlightShowsActivateUntilLitThenReminder)
{
	// a sign or a scroll not lit yet: 0xE83 "Activate"; a lit one: 0xEA0 "Reminder"; both on the action button, with
	// no numbers
	const auto unlit = hand_tooltips::ScriptHighlightToolTip(false);
	EXPECT_EQ(unlit.text, 0xE83u);
	EXPECT_EQ(unlit.action, 2);
	EXPECT_EQ(unlit.align, 0u);
	EXPECT_FALSE(unlit.force);
	EXPECT_EQ(unlit.numbers.count, 0u);

	const auto lit = hand_tooltips::ScriptHighlightToolTip(true);
	EXPECT_EQ(lit.text, 0xEA0u);
	EXPECT_EQ(lit.action, 2);
	EXPECT_EQ(lit.align, 0u);
	EXPECT_FALSE(lit.force);
}

TEST(HandToolTips, OutOfTheInfluenceOnlyAScriptHighlightKeepsItsOwnText)
{
	// out of the influence every other object shows the land's tooltips; a script highlight does not need the influence
	EXPECT_TRUE(hand_tooltips::OverObjectShowsLand(false, true));
	EXPECT_FALSE(hand_tooltips::OverObjectShowsLand(false, false));
	EXPECT_FALSE(hand_tooltips::OverObjectShowsLand(true, true));
	EXPECT_FALSE(hand_tooltips::OverObjectShowsLand(true, false));
}

TEST(HandToolTips, AHelpBubbleOffersScrollOnTheActionButtonWithTheUpAndDownArrows)
{
	const auto tip = hand_tooltips::BubbleToolTip();
	EXPECT_EQ(tip.text, 0xE84u);
	EXPECT_EQ(tip.action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(tip.align, 0x300u);
	EXPECT_FALSE(tip.force);
	EXPECT_EQ(tip.numbers.count, 0u);
}

TEST(HandToolTips, TheEmptyHandTheCameraAndAHeldThingShowTheCameraIconsUnlessScooping)
{
	// the empty hand, the camera's, holding something and a spell seed show them
	EXPECT_TRUE(hand_tooltips::ShowsCameraIcons(1, false));
	EXPECT_TRUE(hand_tooltips::ShowsCameraIcons(2, false));
	EXPECT_TRUE(hand_tooltips::ShowsCameraIcons(4, false));
	EXPECT_TRUE(hand_tooltips::ShowsCameraIcons(8, false));
	// a held thing's locked select scooping holds them back; the empty hand does not care
	EXPECT_FALSE(hand_tooltips::ShowsCameraIcons(4, true));
	EXPECT_FALSE(hand_tooltips::ShowsCameraIcons(8, true));
	EXPECT_TRUE(hand_tooltips::ShowsCameraIcons(1, true));
	// the hidden hand, the tug, the totem, the creature, an animation and the temple never
	for (const int32_t state : {0, 3, 5, 6, 7, 9, 0xA})
	{
		EXPECT_FALSE(hand_tooltips::ShowsCameraIcons(state, false)) << state;
	}
}

TEST(HandToolTips, AnObjectTheHandMayActOnHoldsTheCrossBack)
{
	// interactable, in the influence or not needing it
	EXPECT_TRUE(hand_tooltips::HoldsCrossBack(true, true, true));
	EXPECT_TRUE(hand_tooltips::HoldsCrossBack(true, false, false));
	EXPECT_TRUE(hand_tooltips::HoldsCrossBack(true, true, false));
	// out of the influence when it needs it, or not interactable
	EXPECT_FALSE(hand_tooltips::HoldsCrossBack(true, false, true));
	EXPECT_FALSE(hand_tooltips::HoldsCrossBack(false, true, true));
	EXPECT_FALSE(hand_tooltips::HoldsCrossBack(false, true, false));
}

TEST(HandToolTips, GrippingTheLandWithTheActionButtonHeldIsZoomLandscapeWhenTheZoomIsAllowed)
{
	EXPECT_EQ(hand_tooltips::LandGripHandState(false, 0x1BF), 20);
	EXPECT_EQ(hand_tooltips::LandGripHandState(true, 0x1BF), 22);
	// without the zoom feature, the grip stays Grip Landscape
	EXPECT_EQ(hand_tooltips::LandGripHandState(true, 0x1BF & ~4u), 20);
	const auto tip = hand_tooltips::ZoomLandscapeToolTip();
	EXPECT_EQ(tip.text, 0xEFAu);
	EXPECT_EQ(tip.action, 6);
	EXPECT_EQ(tip.align, 0x300u);
	EXPECT_FALSE(tip.force);
}

TEST(HandToolTips, TheLeashOffersDetachOverItsTieAttachOverAThingAndBringCreatureOverNothing)
{
	using Kind = hand_tooltips::LeashToolTipKind;
	const hand_tooltips::LeashFacts on {.leashOn = true, .hasCreature = true, .creaturePhase = 4};
	// the leash off: nothing, even over nothing
	EXPECT_EQ(hand_tooltips::LeashToolTipOf({}), Kind::None);
	// tied: "Detach Leash" only over the thing tied to
	auto tied = on;
	tied.tied = true;
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(tied), Kind::None);
	tied.hasObject = true;
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(tied), Kind::None);
	tied.overTiedObject = true;
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(tied), Kind::Detach);
	// untied over nothing: "Bring Creature"
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(on), Kind::BringCreature);
	// untied over a thing: "Attach Leash" once the creature is in its fourth stage
	auto over = on;
	over.hasObject = true;
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(over), Kind::Attach);
	over.creaturePhase = 3;
	EXPECT_EQ(hand_tooltips::LeashToolTipOf(over), Kind::None);
	over.creaturePhase = 4;
	// never over the creature itself, a script highlight, a one-shot orb or a spell icon
	for (const auto& [isCreature, valid, orb] :
	     {std::tuple(true, true, false), std::tuple(false, false, false), std::tuple(false, true, true)})
	{
		auto thing = over;
		thing.objectIsCreature = isCreature;
		thing.validAsInterfaceTarget = valid;
		thing.orbOrSpellIcon = orb;
		EXPECT_EQ(hand_tooltips::LeashToolTipOf(thing), Kind::None);
	}
	// the texts, all on the action button
	EXPECT_FALSE(hand_tooltips::LeashToolTip(Kind::None).has_value());
	EXPECT_EQ(hand_tooltips::LeashToolTip(Kind::Detach)->text, 0xEA3u);
	EXPECT_EQ(hand_tooltips::LeashToolTip(Kind::Attach)->text, 0xEA4u);
	EXPECT_EQ(hand_tooltips::LeashToolTip(Kind::BringCreature)->text, 0xEA2u);
	EXPECT_EQ(hand_tooltips::LeashToolTip(Kind::BringCreature)->action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(hand_tooltips::LeashToolTip(Kind::Attach)->align, 0u);
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

TEST(HandToolTips, CanPickUpShowsTheLandUnderTheScriptsLimitsThenCatchOverAThingInThePhysicsAboveNothing)
{
	using hand_tooltips::Shows;
	// The script's interface interaction limits the interface: the land's tooltips, whatever the thing
	for (const uint8_t flags : {uint8_t {2}, uint8_t {6}, uint8_t {3}})
	{
		const auto land = hand_tooltips::CanPickUpFirst(flags, true, 5.0f);
		ASSERT_TRUE(land.has_value());
		EXPECT_EQ(land->shows, Shows::Land);
	}
	// Only the 2 does: 0 (active), 1 (inactive) and 4 (the hand's interaction alone) go on
	for (const uint8_t flags : {uint8_t {0}, uint8_t {1}, uint8_t {4}})
	{
		const auto own = hand_tooltips::CanPickUpFirst(flags, false, 5.0f);
		EXPECT_FALSE(own.has_value());
	}
	// In the physics at nothing or above: 0xE80 "Catch" on the action button, not forced
	for (const float altitude : {0.0f, 12.5f})
	{
		const auto catchIt = hand_tooltips::CanPickUpFirst(0, true, altitude);
		ASSERT_TRUE(catchIt.has_value());
		EXPECT_EQ(catchIt->shows, Shows::Own);
		EXPECT_EQ(catchIt->tip.text, 0xE80u);
		EXPECT_EQ(catchIt->tip.action, hand_tooltips::k_ActionRight);
		EXPECT_EQ(catchIt->tip.align, 0u);
		EXPECT_FALSE(catchIt->tip.force);
	}
	// Below nothing, or no number, the thing's own tooltips
	EXPECT_FALSE(hand_tooltips::CanPickUpFirst(0, true, -0.25f).has_value());
	EXPECT_FALSE(hand_tooltips::CanPickUpFirst(0, true, std::numeric_limits<float>::quiet_NaN()).has_value());
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
	const auto tip = hand_tooltips::SpellIconToolTip(0xE94, std::nullopt);
	EXPECT_EQ(tip.text, 0xE94u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_TRUE(tip.force);
	EXPECT_FALSE(tip.numbers.percentAfter.has_value());
	EXPECT_EQ(hand_tooltips::SpellIconToolTip(0xE94, 25.0).numbers.percentAfter, 25.0);
	// a seed with no tooltip submits a text the help system drops, so the icon shows nothing, not even the land's
	EXPECT_LT(hand_tooltips::SpellIconToolTip(0, std::nullopt).text, openblack::help::tooltips::k_First);
}

TEST(HandToolTips, ASpellIconsChargeIsItsStoreOverWhatItNeedsInFloats)
{
	// the original's own arithmetic, run on its code: store, required -> the double written after the name
	const std::vector<std::tuple<float, float, uint64_t>> cases {
	    {1.0f, 8.0f, 0x4029000000000000u},   {3.0f, 24.0f, 0x4029000000000000u},  {25.0f, 100.0f, 0x4039000000000000u},
	    {1.0f, 3.0f, 0x4040AAAAC0000000u},   {2.0f, 3.0f, 0x4050AAAAC0000000u},   {99.5f, 100.0f, 0x4058E00000000000u},
	    {1.0f, 200.0f, 0x3FE0000000000000u}, {1.0f, 800.0f, 0x3FC0000000000000u}, {1.0f, 7.0f, 0x402C924940000000u},
	    {1.0f, 0.0f, 0x7FF0000000000000u},
	};
	for (const auto& [store, required, bits] : cases)
	{
		const auto percent = hand_tooltips::SpellIconChargePercent(store, required);
		ASSERT_TRUE(percent.has_value()) << store << "/" << required;
		EXPECT_EQ(std::bit_cast<uint64_t>(*percent), bits) << store << "/" << required;
	}
	// none, the name alone: uncharged, and nothing over nothing
	EXPECT_FALSE(hand_tooltips::SpellIconChargePercent(0.0f, 100.0f).has_value());
	EXPECT_FALSE(hand_tooltips::SpellIconChargePercent(0.0f, 0.0f).has_value());
	// a tiny charge is still shown (as "  0%")
	EXPECT_TRUE(hand_tooltips::SpellIconChargePercent(1e-6f, 100.0f).has_value());
}

TEST(HandToolTips, ABuiltWorkshopShowsHowFarItsScaffoldHasGot)
{
	// a workshop being built goes on to the entrance, the storage pit and the tap
	EXPECT_FALSE(hand_tooltips::WorkshopToolTip(false, true, 100, 200.0f).has_value());

	const auto tip = hand_tooltips::WorkshopToolTip(true, true, 100, 200.0f);
	ASSERT_TRUE(tip.has_value());
	EXPECT_EQ(tip->text, 0xEF3u);
	EXPECT_EQ(tip->action, -1);
	EXPECT_EQ(tip->align, 0u);
	EXPECT_FALSE(tip->force);
	ASSERT_EQ(tip->numbers.count, 1u);
	EXPECT_EQ(tip->numbers.values[0], 50.0);
	// a damaged one shows its text as it is written
	EXPECT_EQ(hand_tooltips::WorkshopToolTip(true, false, 100, 200.0f)->numbers.count, 0u);
}

TEST(HandToolTips, AWorkshopsProgressIsCutAndTakenModuloAHundredAsTheOriginals)
{
	// the original's own arithmetic, run on its code: turns left of the 200 a scaffold takes -> the percent shown
	const std::vector<std::pair<int32_t, int32_t>> cases {
	    {0, 0},    {1, 99},   {2, 99},   {3, 98},  {7, 96},  {50, 75}, {99, 50}, {100, 50},
	    {101, 49}, {133, 33}, {150, 25}, {198, 1}, {199, 0}, {200, 0}, {201, 0}, {300, -50},
	};
	for (const auto& [left, shown] : cases)
	{
		EXPECT_EQ(hand_tooltips::WorkshopProductionPercent(left, 200.0f), shown) << left;
	}
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

namespace
{
// the camera's hints and features, by the values the camera's mouse uses (camera_drag)
constexpr uint32_t k_RotateHint = 0x01;
constexpr uint32_t k_PitchHint = 0x02;
constexpr uint32_t k_PitchFeature = 0x01;
constexpr uint32_t k_RotateFeature = 0x02;
constexpr uint32_t k_ZoomFeature = 0x04;
constexpr uint32_t k_AllFeatures = k_PitchFeature | k_RotateFeature | k_ZoomFeature;
} // namespace

TEST(HandToolTips, OverTheLandTheHandOffersToMoveOnTheLeftButtonWithAllArrows)
{
	const auto tip = hand_tooltips::LandToolTip({.tricons = 0, .features = k_AllFeatures});
	EXPECT_EQ(tip.text, 0xE7Eu);
	EXPECT_EQ(tip.action, hand_tooltips::k_ActionLeft);
	EXPECT_EQ(tip.align, hand_tooltips::k_ArrowsAll);
	EXPECT_FALSE(tip.force);
}

TEST(HandToolTips, TheTiltHintComesBeforeTheTurnHint)
{
	const auto both = hand_tooltips::LandToolTip({.tricons = k_PitchHint | k_RotateHint, .features = k_AllFeatures});
	EXPECT_EQ(both.text, 0xE76u);
	EXPECT_EQ(both.action, hand_tooltips::k_ActionLeft);
	EXPECT_EQ(both.align, hand_tooltips::k_ArrowsUpDown);
	const auto turn = hand_tooltips::LandToolTip({.tricons = k_RotateHint, .features = k_AllFeatures});
	EXPECT_EQ(turn.text, 0xE77u);
	EXPECT_EQ(turn.action, hand_tooltips::k_ActionLeft);
	EXPECT_EQ(turn.align, hand_tooltips::k_ArrowsAll);
}

TEST(HandToolTips, ACameraControlTheScriptsStopIsNotOffered)
{
	// a lesson that stops the tilt leaves the turn, and one that stops both leaves the move
	EXPECT_EQ(hand_tooltips::LandToolTip({.tricons = k_PitchHint | k_RotateHint, .features = k_RotateFeature}).text, 0xE77u);
	EXPECT_EQ(hand_tooltips::LandToolTip({.tricons = k_PitchHint | k_RotateHint, .features = k_ZoomFeature}).text, 0xE7Eu);
	// the pitch feature does not offer the turn
	EXPECT_EQ(hand_tooltips::LandToolTip({.tricons = k_RotateHint, .features = k_PitchFeature}).text, 0xE7Eu);
}

TEST(HandToolTips, AHintComesBeforeAFishAndAFishBeforeTheMove)
{
	const auto fish = hand_tooltips::LandToolTip({.tricons = 0, .features = k_AllFeatures, .fishInReach = true});
	EXPECT_EQ(fish.text, 0xE73u);
	EXPECT_EQ(fish.action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(fish.align, 0u);
	EXPECT_EQ(hand_tooltips::LandToolTip({.tricons = k_RotateHint, .features = k_AllFeatures, .fishInReach = true}).text,
	          0xE77u);
}

TEST(HandToolTips, ZoomOutNearTheLandWithTheCameraLookingDown)
{
	hand_tooltips::LandFacts near {.features = k_AllFeatures, .centreOnLand = true, .centreDistance = 14.9f, .pitch = 0.6f};
	const auto tip = hand_tooltips::LandToolTip(near);
	EXPECT_EQ(tip.text, 0xE8Bu);
	// the zoom-in control's picture, and no arrows
	EXPECT_EQ(tip.action, hand_tooltips::k_ActionZoomIn);
	EXPECT_EQ(tip.align, 0u);
	// both tests are strict
	auto far = near;
	far.centreDistance = 15.0f;
	EXPECT_EQ(hand_tooltips::LandToolTip(far).text, 0xE7Eu);
	auto level = near;
	level.pitch = 0.55f;
	EXPECT_EQ(hand_tooltips::LandToolTip(level).text, 0xE7Eu);
	// with no land at the middle of the screen, or no zoom allowed, the hand offers the move
	auto sky = near;
	sky.centreOnLand = false;
	EXPECT_EQ(hand_tooltips::LandToolTip(sky).text, 0xE7Eu);
	auto noZoom = near;
	noZoom.features = k_PitchFeature | k_RotateFeature;
	EXPECT_EQ(hand_tooltips::LandToolTip(noZoom).text, 0xE7Eu);
}

TEST(HandToolTips, TheHintsAndTheFishComeBeforeZoomOut)
{
	hand_tooltips::LandFacts near {.features = k_AllFeatures, .centreOnLand = true, .centreDistance = 10.0f, .pitch = 1.0f};
	near.tricons = k_RotateHint;
	EXPECT_EQ(hand_tooltips::LandToolTip(near).text, 0xE77u);
	near.tricons = 0;
	near.fishInReach = true;
	EXPECT_EQ(hand_tooltips::LandToolTip(near).text, 0xE73u);
}

TEST(HandToolTips, ZoomOutShowsTheWheelOfTheZoomInControl)
{
	int32_t animType = -2;
	int32_t clickType = -2;
	int32_t row = -2;
	std::u16string keyName;
	// the zoom-in control is the wheel up, the zoom-out control the wheel down, both on the mouse's wheel row
	ASSERT_TRUE(help::input_prompt::ResolveAction(hand_tooltips::k_ActionZoomIn, animType, clickType, row, keyName));
	EXPECT_EQ(animType, 0);
	EXPECT_EQ(clickType, 0x10);
	EXPECT_EQ(row, help::input_prompt::MouseRow());
	EXPECT_TRUE(keyName.empty());
	ASSERT_TRUE(help::input_prompt::ResolveAction(3, animType, clickType, row, keyName));
	EXPECT_EQ(clickType, 8);
}

TEST(HandToolTips, OnAMouseWithNoWheelTheZoomControlsShowBothButtons)
{
	namespace prompt = help::input_prompt;
	// with a wheel: the wheel up and down
	EXPECT_EQ(prompt::MouseClickTypeOf(hand_tooltips::k_ActionZoomIn, true), std::optional<int32_t>(0x10));
	EXPECT_EQ(prompt::MouseClickTypeOf(3, true), std::optional<int32_t>(8));
	// with no wheel the zoom controls have no mouse binding, and show both buttons, as the left and right actions are
	// the two buttons
	EXPECT_EQ(prompt::MouseClickTypeOf(hand_tooltips::k_ActionZoomIn, false), std::optional<int32_t>(3));
	EXPECT_EQ(prompt::MouseClickTypeOf(3, false), std::optional<int32_t>(3));
	// the buttons keep theirs, and a key binding has no mouse picture
	EXPECT_EQ(prompt::MouseClickTypeOf(hand_tooltips::k_ActionLeft, false), std::optional<int32_t>(1));
	EXPECT_EQ(prompt::MouseClickTypeOf(hand_tooltips::k_ActionRight, false), std::optional<int32_t>(2));
	EXPECT_FALSE(prompt::MouseClickTypeOf(0, false).has_value());
}

TEST(HandToolTips, ABuildingThatIsNotWholeShowsTheWoodItWantsThenHowFarItIsBuilt)
{
	const hand_tooltips::BuildingFacts site {.built = false,
	                                         .repaired = true,
	                                         .ownOrNoPlayer = true,
	                                         .pitchAllowed = true,
	                                         .percentBuilt = 0.25f,
	                                         .percentRepaired = 1.0f,
	                                         .siteWood = 42.5f};
	const auto tips = hand_tooltips::BuildingToolTips(site);
	ASSERT_TRUE(tips.has_value());
	EXPECT_EQ((*tips)[0].text, 0xEFBu);
	EXPECT_EQ((*tips)[0].action, hand_tooltips::k_ActionNone);
	EXPECT_EQ((*tips)[0].numbers.count, 1u);
	EXPECT_DOUBLE_EQ((*tips)[0].numbers.values[0], 42.5);
	EXPECT_FALSE((*tips)[0].numbers.text.has_value());
	EXPECT_EQ((*tips)[1].text, 0xEF4u);
	EXPECT_EQ((*tips)[1].action, hand_tooltips::k_ActionNone);
	EXPECT_DOUBLE_EQ((*tips)[1].numbers.values[0], 25.0);
}

TEST(HandToolTips, AWholeBuildingAnotherPlayersOrAStoppedTiltShowNoBuildingTexts)
{
	const hand_tooltips::BuildingFacts site {.built = false, .ownOrNoPlayer = true, .pitchAllowed = true, .siteWood = 1.0f};
	auto whole = site;
	whole.built = true;
	whole.repaired = true;
	EXPECT_FALSE(hand_tooltips::BuildingToolTips(whole).has_value());
	auto theirs = site;
	theirs.ownOrNoPlayer = false;
	EXPECT_FALSE(hand_tooltips::BuildingToolTips(theirs).has_value());
	auto noTilt = site;
	noTilt.pitchAllowed = false;
	EXPECT_FALSE(hand_tooltips::BuildingToolTips(noTilt).has_value());
}

TEST(HandToolTips, ADamagedBuildingShowsHowFarItIsRepairedAndNoSiteShowsItTwice)
{
	// built and not repaired: the life, and with no site the wood's text is written as "Building Completed"
	const hand_tooltips::BuildingFacts damaged {.built = true,
	                                            .repaired = false,
	                                            .ownOrNoPlayer = true,
	                                            .pitchAllowed = true,
	                                            .percentBuilt = 1.0f,
	                                            .percentRepaired = 0.5f};
	const auto tips = hand_tooltips::BuildingToolTips(damaged);
	ASSERT_TRUE(tips.has_value());
	EXPECT_EQ((*tips)[0].text, 0xEFBu);
	EXPECT_EQ((*tips)[0].numbers.text, std::optional<uint32_t>(0xEF4u));
	EXPECT_DOUBLE_EQ((*tips)[0].numbers.values[0], 50.0);
	EXPECT_DOUBLE_EQ((*tips)[1].numbers.values[0], 50.0);
	// a site that wants no more wood, or less than none, shows 0
	auto done = damaged;
	done.siteWood = -3.0f;
	EXPECT_DOUBLE_EQ((*hand_tooltips::BuildingToolTips(done))[0].numbers.values[0], 0.0);
	// the percent is the float times 100 in double precision
	auto odd = damaged;
	odd.percentRepaired = 0.755f;
	EXPECT_DOUBLE_EQ((*hand_tooltips::BuildingToolTips(odd))[1].numbers.values[0], static_cast<double>(0.755f) * 100.0);
}

TEST(HandToolTips, ASpellIconWithAWorshipSitesIconSkipsTheBuildingTexts)
{
	// a worship site's icon, or a town centre's whose site has one of its seed: no building texts at all
	EXPECT_FALSE(hand_tooltips::SpellIconOwnOrNoPlayer(true, false, std::nullopt).has_value());
	EXPECT_FALSE(hand_tooltips::SpellIconOwnOrNoPlayer(true, true, true).has_value());
	// a town centre's: its town's player, or of no player without a town
	EXPECT_EQ(hand_tooltips::SpellIconOwnOrNoPlayer(false, true, true), std::optional<bool>(true));
	EXPECT_EQ(hand_tooltips::SpellIconOwnOrNoPlayer(false, true, false), std::optional<bool>(false));
	EXPECT_EQ(hand_tooltips::SpellIconOwnOrNoPlayer(false, true, std::nullopt), std::optional<bool>(true));
	// any other icon: the neutral player's
	EXPECT_EQ(hand_tooltips::SpellIconOwnOrNoPlayer(false, false, std::nullopt), std::optional<bool>(false));
}

TEST(HandToolTips, HoldingAThingOverACreatureThatTakesItOffersGiveOnTheActionButton)
{
	const auto tip = hand_tooltips::GiveToolTip();
	EXPECT_EQ(tip.text, 0xE87u);
	EXPECT_EQ(tip.action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_FALSE(tip.force);
}

TEST(HandToolTips, HoldingAScaffoldOverOneItMayJoinOffersCombineScaffoldsOnTheActionButton)
{
	const auto tip = hand_tooltips::CombineScaffoldsToolTip();
	EXPECT_EQ(tip.text, 0xEA5u);
	EXPECT_EQ(tip.action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(tip.align, 0u);
	EXPECT_FALSE(tip.force);
	EXPECT_EQ(tip.numbers.count, 0u);
}

TEST(HandToolTips, AHeldTreeOrScaffoldShowsItsOwnDropText)
{
	// a tree "Plant"; a scaffold its own (its plan's, or the plain scaffold's); anything else none
	EXPECT_EQ(hand_tooltips::HeldDropText(true, std::nullopt), 0xEEFu);
	EXPECT_EQ(hand_tooltips::HeldDropText(false, 0xEF0u), 0xEF0u);
	EXPECT_EQ(hand_tooltips::HeldDropText(false, 0xE91u), 0xE91u);
	EXPECT_EQ(hand_tooltips::HeldDropText(false, std::nullopt), 0u);
}

TEST(HandToolTips, WithNothingToGiveOrApplyTheHandOffersItsDropTextThenThrow)
{
	const auto plain = hand_tooltips::DropToolTips(0);
	EXPECT_EQ(plain[0].text, 0xEEEu);
	EXPECT_EQ(plain[0].action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(plain[0].align, 0u);
	EXPECT_FALSE(plain[0].force);
	EXPECT_EQ(plain[1].text, 0xE74u);
	EXPECT_EQ(plain[1].action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(plain[1].align, hand_tooltips::k_ArrowsAll);
	EXPECT_FALSE(plain[1].force);
	// a held object's own drop text takes the place of "Drop"; "Throw" stays
	const auto scaffold = hand_tooltips::DropToolTips(0xEF0);
	EXPECT_EQ(scaffold[0].text, 0xEF0u);
	EXPECT_EQ(scaffold[1].text, 0xE74u);
}

namespace
{
// Every test that could pass reads true, so each case shows which one comes first
hand_tooltips::HeldFacts AllHeldFacts()
{
	return {
	    .inInfluence = true,
	    .overTarget = true,
	    .bothScaffolds = false,
	    .scaffoldsJoin = true,
	    .takesGift = true,
	    .validToApply = true,
	    .ownDrop = 0xEF0,
	};
}

std::vector<uint32_t> TextsOf(const hand_tooltips::HeldChoice& choice)
{
	std::vector<uint32_t> texts;
	for (size_t i = 0; i < choice.count; ++i)
	{
		texts.push_back(choice.tips.at(i).text);
	}
	return texts;
}
} // namespace

TEST(HandToolTips, WithAThingInTheHandOutOfTheInfluenceTheLandTextsShow)
{
	auto facts = AllHeldFacts();
	facts.inInfluence = false;
	EXPECT_EQ(hand_tooltips::HeldToolTips(facts).shows, hand_tooltips::Shows::Land);
}

TEST(HandToolTips, TwoScaffoldsThatJoinOfferOnlyCombineScaffolds)
{
	auto facts = AllHeldFacts();
	facts.bothScaffolds = true;
	const auto choice = hand_tooltips::HeldToolTips(facts);
	EXPECT_EQ(choice.shows, hand_tooltips::Shows::Own);
	EXPECT_EQ(TextsOf(choice), std::vector<uint32_t>({0xEA5u}));
	EXPECT_EQ(choice.tips[0].action, hand_tooltips::k_ActionRight);
}

TEST(HandToolTips, TwoScaffoldsThatDoNotJoinSkipTheGiftAndTheApplyForTheDropTexts)
{
	auto facts = AllHeldFacts();
	facts.bothScaffolds = true;
	facts.scaffoldsJoin = false;
	const auto choice = hand_tooltips::HeldToolTips(facts);
	EXPECT_EQ(choice.shows, hand_tooltips::Shows::Own);
	EXPECT_EQ(TextsOf(choice), std::vector<uint32_t>({0xEF0u, 0xE74u}));
	EXPECT_EQ(choice.tips[1].align, hand_tooltips::k_ArrowsAll);
}

TEST(HandToolTips, WithAThingInTheHandTheGiftComesBeforeTheApply)
{
	const auto gift = hand_tooltips::HeldToolTips(AllHeldFacts());
	EXPECT_EQ(TextsOf(gift), std::vector<uint32_t>({0xE87u}));

	auto facts = AllHeldFacts();
	facts.takesGift = false;
	const auto apply = hand_tooltips::HeldToolTips(facts);
	EXPECT_EQ(TextsOf(apply), std::vector<uint32_t>({0xE8Eu}));
	EXPECT_EQ(apply.tips[0].action, hand_tooltips::k_ActionRight);
	EXPECT_EQ(apply.tips[0].align, 0u);
	// over a worship site's altar what is applied is sacrificed
	facts.overSacrificeAltar = true;
	const auto sacrifice = hand_tooltips::HeldToolTips(facts);
	EXPECT_EQ(TextsOf(sacrifice), std::vector<uint32_t>({0xE8Du}));
	EXPECT_EQ(sacrifice.tips[0].action, hand_tooltips::k_ActionRight);
}

TEST(HandToolTips, WithNothingToGiveOrApplyToOrNoTargetTheDropTextsShow)
{
	auto facts = AllHeldFacts();
	facts.takesGift = false;
	facts.validToApply = false;
	facts.ownDrop = 0;
	EXPECT_EQ(TextsOf(hand_tooltips::HeldToolTips(facts)), std::vector<uint32_t>({0xEEEu, 0xE74u}));

	// with nothing under the hand none of the target's tests count
	auto noTarget = AllHeldFacts();
	noTarget.overTarget = false;
	noTarget.bothScaffolds = true;
	EXPECT_EQ(TextsOf(hand_tooltips::HeldToolTips(noTarget)), std::vector<uint32_t>({0xEF0u, 0xE74u}));
}

TEST(HandToolTips, TheHandTakesATotemPartATappableThingACreatureOrANeedSign)
{
	EXPECT_FALSE(hand_tooltips::IsInterfaceObject({}));
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.totemPart = true}));
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.tappable = true}));
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.creature = true}));
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.needsSign = true}));
}

TEST(HandToolTips, ADesireFlagIsTheHandsObject)
{
	// interactable, no locked select and not for the hand: Over Object (18), whose text is the flag's need
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.desireFlag = true}));
	EXPECT_EQ(hand_tooltips::ObjectHandState({.interactable = true, .inInfluence = true, .mustBeInInfluence = true}), 18);
}

TEST(HandToolTips, AnAltarIsTheHandsObject)
{
	// interactable, no locked select and not for the hand: Over Object (18), whose text is its site's prayer energy
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.altar = true}));
	EXPECT_EQ(hand_tooltips::ObjectHandState({.interactable = true, .inInfluence = true, .mustBeInInfluence = true}), 18);
}

TEST(HandToolTips, TheTempleIsTheHandsObjectAndShowsItsHealthWithNoButton)
{
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.templeHeart = true}));
	const auto tip = hand_tooltips::TempleHealthToolTip(0.5f);
	EXPECT_EQ(tip.text, 0xEE8u);
	EXPECT_EQ(tip.action, -1);
	EXPECT_EQ(tip.align, 0u);
	ASSERT_EQ(tip.numbers.count, 1u);
	EXPECT_EQ(tip.numbers.values[0], 50.0);
	// its life times 100 in floats: a third of a life is 33.333332
	EXPECT_EQ(hand_tooltips::TempleHealthToolTip(1.0f / 3.0f).numbers.values[0], static_cast<double>((1.0f / 3.0f) * 100.0f));
}

TEST(HandToolTips, AWorshipSiteIsTheHandsObject)
{
	// Over Object (18): the local player's site that is not whole shows its building texts
	EXPECT_TRUE(hand_tooltips::IsInterfaceObject({.worshipSite = true}));
	EXPECT_EQ(hand_tooltips::ObjectHandState({.interactable = true, .inInfluence = true, .mustBeInInfluence = true}), 18);
}

TEST(HandToolTips, AScaffoldOffersItsValueInPlaceOfPickUp)
{
	const auto pickUp = hand_tooltips::PickUpToolTip(std::nullopt);
	EXPECT_EQ(pickUp.text, 0xE73u);
	EXPECT_EQ(pickUp.action, 2);
	EXPECT_FALSE(pickUp.force);
	EXPECT_EQ(pickUp.numbers.count, 0u);
	const auto scaffold = hand_tooltips::PickUpToolTip(3u);
	EXPECT_EQ(scaffold.text, 0xEE7u);
	EXPECT_EQ(scaffold.action, 2);
	EXPECT_FALSE(scaffold.force);
	ASSERT_EQ(scaffold.numbers.count, 1u);
	EXPECT_EQ(scaffold.numbers.values[0], 3.0);
}

TEST(HandToolTips, APileShowsItsFoodOrWoodAndAFieldOffersPickUp)
{
	// a food pile "Food Amount", a wood pile "Wood Amount", with the pile's amount, on the action button
	const auto food = hand_tooltips::PileToolTip(true, 250);
	EXPECT_EQ(food.text, 0xEFDu);
	EXPECT_EQ(food.action, 2);
	EXPECT_EQ(food.align, 0u);
	EXPECT_FALSE(food.force);
	ASSERT_EQ(food.numbers.count, 1u);
	EXPECT_EQ(food.numbers.values[0], 250.0);
	const auto wood = hand_tooltips::PileToolTip(false, 70000);
	EXPECT_EQ(wood.text, 0xEFEu);
	EXPECT_EQ(wood.action, 2);
	EXPECT_FALSE(wood.force);
	ASSERT_EQ(wood.numbers.count, 1u);
	EXPECT_EQ(wood.numbers.values[0], 70000.0);
	// a field "Pick Up", with no number
	const auto field = hand_tooltips::FieldToolTip();
	EXPECT_EQ(field.text, 0xE73u);
	EXPECT_EQ(field.action, 2);
	EXPECT_EQ(field.align, 0u);
	EXPECT_FALSE(field.force);
	EXPECT_EQ(field.numbers.count, 0u);
}

TEST(HandToolTips, ARockAndARewardChestHaveTheirOwnTapTexts)
{
	EXPECT_EQ(hand_tooltips::TapToolTip(false, false).text, 0xE7Au);
	EXPECT_EQ(hand_tooltips::TapToolTip(true, false).text, 0xEF7u);
	EXPECT_EQ(hand_tooltips::TapToolTip(false, true).text, 0xEF8u);
	EXPECT_EQ(hand_tooltips::TapToolTip(false, true).action, 2);
	EXPECT_FALSE(hand_tooltips::TapToolTip(false, true).force);
}
