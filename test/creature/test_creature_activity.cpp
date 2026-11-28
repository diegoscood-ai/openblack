/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureActivity.h"
#include "Creature/CreatureDecisionTree.h"

using namespace openblack;
using namespace openblack::creature_activity;
using creature_desires::Desire;

// Only towns, forests, temples, creatures and flocks are whole things
TEST(CreatureActivity, TheWholeThingsAreTownsForestsTemplesCreaturesAndFlocks)
{
	namespace types = creature_tree::belief_types;
	EXPECT_EQ(WholeOf(types::k_Town), Whole::Town);
	EXPECT_EQ(WholeOf(types::k_Forest), Whole::Forest);
	EXPECT_EQ(WholeOf(types::k_Citadel), Whole::Citadel);
	EXPECT_EQ(WholeOf(types::k_Creature), Whole::Creature);
	EXPECT_EQ(WholeOf(types::k_Flock), Whole::Flock);
	EXPECT_EQ(WholeOf(types::k_Villager), std::nullopt);
	EXPECT_EQ(WholeOf(types::k_Abode), std::nullopt);
	EXPECT_EQ(WholeOf(types::k_CitadelPart), std::nullopt);
}

// The desire to impress is about a town, but not a town of its own player that believes in that player
TEST(CreatureActivity, ItImpressesTownsThatAreNotLoyalToItsPlayer)
{
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .samePlayer = true}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .believesInPlayer = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Town, .samePlayer = true, .believesInPlayer = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Creature}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Impress, {.kind = Whole::Forest}, false));
}

// Anger is about the things of other players, a town of its own player only while the player looks at it
TEST(CreatureActivity, AngerIsAboutTheThingsOfOtherPlayers)
{
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .hasPlayer = true}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Flock}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Citadel, .hasPlayer = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Creature}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Creature, .attackable = true}, false));
	// Its own player's things: never while it must not attack them
	EXPECT_FALSE(
	    CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true, .hasPlayer = true, .lookedAt = true}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest, .samePlayer = true}, false));
	// Then a town only while the player looks, and never its player's temple
	EXPECT_TRUE(
	    CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true, .hasPlayer = true, .lookedAt = true}, true));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true, .hasPlayer = true}, true));
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Forest, .samePlayer = true}, true));
	EXPECT_FALSE(CanBeAbout(Desire::Anger, {.kind = Whole::Citadel, .samePlayer = true, .hasPlayer = true}, true));
	// A town of no player always counts once its things may count
	EXPECT_TRUE(CanBeAbout(Desire::Anger, {.kind = Whole::Town, .samePlayer = true}, true));
}

// Compassion is about towns and creatures; the desires about creatures only about creatures
TEST(CreatureActivity, CompassionAndTheDesiresAboutCreatures)
{
	EXPECT_TRUE(CanBeAbout(Desire::Compassion, {.kind = Whole::Town}, false));
	EXPECT_TRUE(CanBeAbout(Desire::Compassion, {.kind = Whole::Creature}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Compassion, {.kind = Whole::Flock}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Compassion, {.kind = Whole::Forest}, false));
	EXPECT_FALSE(CanBeAbout(Desire::Compassion, {.kind = Whole::Citadel}, false));
	for (const auto desire :
	     {Desire::BeFriends, Desire::ObeyCreature, Desire::TellCreature, Desire::EducateFriend, Desire::MissFriend})
	{
		EXPECT_TRUE(CanBeAbout(desire, {.kind = Whole::Creature}, false));
		EXPECT_FALSE(CanBeAbout(desire, {.kind = Whole::Town}, false));
	}
	EXPECT_FALSE(CanBeAbout(Desire::Hunger, {.kind = Whole::Creature}, false));
}

// Compassion for a town gives actions; compassion for a creature gives none
TEST(CreatureActivity, OnlyCompassionForATownGivesActions)
{
	EXPECT_TRUE(CompassionHasActions(Whole::Town));
	EXPECT_FALSE(CompassionHasActions(Whole::Creature));
}

// The most useful value wins, the first of equals, and only one worth more than nothing
TEST(CreatureActivity, TheMostUsefulIsTheFirstOfTheMostAboveNothing)
{
	EXPECT_EQ(MostUseful(std::array {0.2f, 0.5f, 0.5f, 0.1f}), 1u);
	EXPECT_EQ(MostUseful(std::array {0.0f, 0.0f}), std::nullopt);
	EXPECT_EQ(MostUseful(std::array {-1.0f}), std::nullopt);
	EXPECT_EQ(MostUseful(std::array<float, 0> {}), std::nullopt);
}

// The goal is the first of the most worth among the whole things a desire can be about, never the creature itself, and
// only one worth more than nothing
TEST(CreatureActivity, TheGoalIsTheFirstOfTheMostWorthOfTheWholeThingsTheDesireCanBeAbout)
{
	const WholeCandidate town {.facts = {.kind = Whole::Town}, .worth = 0.4f};
	const WholeCandidate otherTown {.facts = {.kind = Whole::Town}, .worth = 0.4f};
	const WholeCandidate flock {.facts = {.kind = Whole::Flock}, .worth = 0.9f};
	const WholeCandidate creature {.facts = {.kind = Whole::Creature}, .worth = 0.6f};
	const WholeCandidate itself {.facts = {.kind = Whole::Creature}, .itself = true, .worth = 0.8f};
	// Compassion is never about a flock, nor about the creature itself; of the two towns worth as much, the first
	EXPECT_EQ(ChooseWhole(Desire::Compassion, std::array {flock, town, otherTown}, false), 1u);
	EXPECT_EQ(ChooseWhole(Desire::Compassion, std::array {itself, town, creature}, false), 2u);
	// Anger may be about the flock
	EXPECT_EQ(ChooseWhole(Desire::Anger, std::array {town, flock}, false), 1u);
	// The desires about other creatures only about another creature
	EXPECT_EQ(ChooseWhole(Desire::BeFriends, std::array {flock, itself, town}, false), std::nullopt);
	EXPECT_EQ(ChooseWhole(Desire::BeFriends, std::array {flock, itself, creature}, false), 2u);
	// A whole thing worth nothing is no goal
	const WholeCandidate worthless {.facts = {.kind = Whole::Town}};
	EXPECT_EQ(ChooseWhole(Desire::Impress, std::array {worthless}, false), std::nullopt);
	EXPECT_EQ(ChooseWhole(Desire::Impress, std::array<WholeCandidate, 0> {}, false), std::nullopt);
}

// Sent by its player to act on a thing, a desire acts on the thing alone when it suits; else on each thing of the same
// whole thing that suits, in the order given (the newest belief first); nothing suits, nothing to act on
TEST(CreatureActivity, SentToAThingADesireActsOnItElseOnThePartsOfItsWholeThingThatSuit)
{
	const std::vector<int> parts {7, 8, 9, 3};
	EXPECT_EQ(SentToActOn<int>(3, parts, [](int thing) { return thing == 3 || thing == 8; }), (std::vector<int> {3}));
	EXPECT_EQ(SentToActOn<int>(3, parts, [](int thing) { return thing >= 8; }), (std::vector<int> {8, 9}));
	EXPECT_TRUE(SentToActOn<int>(3, parts, [](int) { return false; }).empty());
	EXPECT_TRUE(SentToActOn<int>(3, std::vector<int> {}, [](int thing) { return thing != 3; }).empty());
}

TEST(CreatureActivity, ToImpressItActsOnAVillagerThatDoesNotDance)
{
	EXPECT_TRUE(ThingSuits(Desire::Impress, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Villager, .dancing = true}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Villager, .canActOn = false}));
	// A totem statue can be impressed, while its data lets a creature act on it
	EXPECT_TRUE(ThingSuits(Desire::Impress, {.kind = Thing::TotemStatue}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::TotemStatue, .canActOn = false}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::TotemStatue}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::TotemStatue, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::TotemStatue}));
}

// For anger, it acts on what its data lets a creature attack, and on every tree
TEST(CreatureActivity, ForAngerItActsOnWhatItCanAttack)
{
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Villager, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Villager, .attackable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Animal, .attackable = true, .dead = true}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Tree}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Building, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Tree, .canActOn = false}));
}

// For compassion, it acts on a villager that a hand can pick up, or on any other object
TEST(CreatureActivity, ForCompassionItActsOnVillagersAndBuildings)
{
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::Villager, .pickable = false}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Building}));
	EXPECT_TRUE(ThingSuits(Desire::Compassion, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::Spell}));
}

// A plain object, such as a town's desire flag, suits a desire as any object does: by the data of the object, and never
// for rest, fear or friends
TEST(CreatureActivity, APlainObjectSuitsTheDesiresThatAnyObjectSuits)
{
	// The data of the prayer site: a creature can act on it and examine it, but not attack it or play with it
	const ThingFacts flag {.kind = Thing::PlainObject, .inspectable = true};
	EXPECT_TRUE(ThingSuits(Desire::Compassion, flag));
	EXPECT_TRUE(ThingSuits(Desire::Hunger, flag));
	EXPECT_TRUE(ThingSuits(Desire::Curiosity, flag));
	EXPECT_FALSE(ThingSuits(Desire::Anger, flag));
	EXPECT_FALSE(ThingSuits(Desire::Play, flag));
	EXPECT_FALSE(ThingSuits(Desire::Impress, flag));
	EXPECT_FALSE(ThingSuits(Desire::Fear, flag));
	EXPECT_FALSE(ThingSuits(Desire::Tiredness, flag));
	EXPECT_FALSE(ThingSuits(Desire::Rest, flag));
	EXPECT_FALSE(ThingSuits(Desire::BringStuffHome, flag));
	EXPECT_FALSE(ThingSuits(Desire::BeFriends, flag));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, flag));
	// It takes poo only when it is taller than the tallest thing a creature does not poo on
	EXPECT_FALSE(ThingSuits(Desire::Poo, flag));
	EXPECT_TRUE(ThingSuits(Desire::Poo, {.kind = Thing::PlainObject, .height = k_TallestThingNotToPooOn + 0.5f}));
	// Other data can let a creature attack it or play with it
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::PlainObject, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::PlainObject, .attackable = true, .itself = true}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::PlainObject, .playable = true}));
	// Data that does not let a creature act on it refuses every desire
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::PlainObject, .canActOn = false}));
	EXPECT_FALSE(ThingSuits(Desire::Hunger, {.kind = Thing::PlainObject, .canActOn = false}));
	// Anything else that is not an object suits none of the desires that need an object
	EXPECT_FALSE(ThingSuits(Desire::Compassion, {.kind = Thing::Other}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Other, .inspectable = true}));
}

// To get high, it acts only on a magic mushroom
TEST(CreatureActivity, ToGetHighItActsOnlyOnAMagicMushroom)
{
	EXPECT_TRUE(ThingSuits(Desire::GetHigh, {.kind = Thing::Building, .mushroom = true}));
	EXPECT_TRUE(ThingSuits(Desire::GetHigh, {.kind = Thing::MobileObject, .mushroom = true}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Rock}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::MobileObject}));
	EXPECT_FALSE(ThingSuits(Desire::GetHigh, {.kind = Thing::Building, .canActOn = false, .mushroom = true}));
}

// For anger, a rock always suits, a creature that is itself or a friend does not
TEST(CreatureActivity, ForAngerARockSuitsAndAFriendDoesNot)
{
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Rock}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true, .itself = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Creature, .attackable = true, .friendOfCreature = true}));
	EXPECT_TRUE(ThingSuits(Desire::Anger, {.kind = Thing::MobileObject, .attackable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Spell, .attackable = true}));
}

// It plays with what lies about, statics, rocks, trees and creatures, and with living things whose data allows it
TEST(CreatureActivity, ItPlaysWithThingsThatLieAboutAndWithLivingThingsWhoseDataAllowsIt)
{
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::MobileObject}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::MobileStatic}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::Creature}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::Villager, .playable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Animal, .playable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Play, {.kind = Thing::Other}));
}

// Fear is of creatures, miracles and the animals that frighten
TEST(CreatureActivity, FearIsOfCreaturesMiraclesAndAnimalsThatFrighten)
{
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Creature}));
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Spell}));
	EXPECT_TRUE(ThingSuits(Desire::Fear, {.kind = Thing::Animal, .frightening = true}));
	EXPECT_FALSE(ThingSuits(Desire::Fear, {.kind = Thing::Animal}));
	EXPECT_FALSE(ThingSuits(Desire::Fear, {.kind = Thing::Villager}));
}

// It looks over what its data allows and is not dead
TEST(CreatureActivity, ItLooksOverWhatItsDataAllows)
{
	EXPECT_TRUE(ThingSuits(Desire::Curiosity, {.kind = Thing::Building, .inspectable = true}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Building}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Villager, .inspectable = true, .dead = true}));
	EXPECT_FALSE(ThingSuits(Desire::Curiosity, {.kind = Thing::Spell, .inspectable = true}));
}

// It poos only on a tall thing that does not live
TEST(CreatureActivity, ItPoosOnlyOnATallThingThatDoesNotLive)
{
	EXPECT_TRUE(ThingSuits(Desire::Poo, {.kind = Thing::Building, .height = 1.6f}));
	EXPECT_FALSE(ThingSuits(Desire::Poo, {.kind = Thing::Building, .height = k_TallestThingNotToPooOn}));
	EXPECT_FALSE(ThingSuits(Desire::Poo, {.kind = Thing::Villager, .height = 2.0f}));
}

// It rests by a creature or by a thing set into the land, but not by a field or a fish farm
TEST(CreatureActivity, ItRestsByACreatureOrAThingSetIntoTheLand)
{
	for (const auto desire : {Desire::Tiredness, Desire::Rest})
	{
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Creature}));
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Building}));
		EXPECT_TRUE(ThingSuits(desire, {.kind = Thing::Tree}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::Field}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::FishFarm}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::MobileObject}));
		EXPECT_FALSE(ThingSuits(desire, {.kind = Thing::Villager}));
	}
}

// It brings home what it can pick up, and the desires about other creatures act only on creatures
TEST(CreatureActivity, ItBringsHomeWhatItCanPickUpAndBefriendsOnlyCreatures)
{
	EXPECT_TRUE(ThingSuits(Desire::BringStuffHome, {.kind = Thing::MobileObject, .creatureCanPickUp = true}));
	EXPECT_FALSE(ThingSuits(Desire::BringStuffHome, {.kind = Thing::MobileObject}));
	EXPECT_TRUE(ThingSuits(Desire::BeFriends, {.kind = Thing::Creature}));
	EXPECT_FALSE(ThingSuits(Desire::MissFriend, {.kind = Thing::Villager}));
	// A desire without a rule of its own acts on any thing that lets a creature act on it
	EXPECT_TRUE(ThingSuits(Desire::Hunger, {.kind = Thing::Villager}));
	EXPECT_FALSE(ThingSuits(Desire::Hunger, {.kind = Thing::Villager, .canActOn = false}));
}

// A town's totem building and a puzzle's totem may always be impressed and played with; for the rest they are buildings
TEST(CreatureActivity, ATotemMayAlwaysBeImpressedAndPlayedWith)
{
	EXPECT_TRUE(ThingSuits(Desire::Impress, {.kind = Thing::Totem}));
	EXPECT_TRUE(ThingSuits(Desire::Play, {.kind = Thing::Totem}));
	EXPECT_TRUE(ThingSuits(Desire::Rest, {.kind = Thing::Totem}));
	EXPECT_FALSE(ThingSuits(Desire::Anger, {.kind = Thing::Totem}));
	EXPECT_FALSE(ThingSuits(Desire::Impress, {.kind = Thing::Totem, .canActOn = false}));
}
