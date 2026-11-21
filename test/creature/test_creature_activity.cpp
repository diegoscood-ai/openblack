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
