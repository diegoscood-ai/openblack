/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's leash tie rules (Creature/LeashTie.h): which things the leash may be tied to, the double click's
// decision in the original's order, and the single tap with the leash in the hand

#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "Creature/LeashTie.h"

using namespace openblack;
using namespace openblack::creature_leash;

namespace
{
constexpr std::array<TieTargetKind, 5> k_Kinds = {TieTargetKind::Object, TieTargetKind::LeashPost, TieTargetKind::SpellIcon,
                                                  TieTargetKind::ScriptHighlight, TieTargetKind::OneOffSpellSeed};

/// A double click on a tree, with the leash in the player's hand and nothing tied
HandTieCheck OnATree()
{
	return {
	    .hasTarget = true,
	    .kind = TieTargetKind::Object,
	    .hasCreature = true,
	    .leashInThisHand = true,
	};
}

/// A tap with the leash on, untied, in the player's hand
LeashTapCheck LeashHeld()
{
	return {.leashOnUntied = true, .hasCreature = true, .leashInThisHand = true};
}
} // namespace

TEST(LeashTie, OnlyAScriptHighlightIsNoTarget)
{
	for (const auto kind : k_Kinds)
	{
		EXPECT_EQ(ValidAsTarget(kind), kind != TieTargetKind::ScriptHighlight);
	}
}

TEST(LeashTie, TheLeashCannotBeTiedToAPostOrASpellIcon)
{
	for (const auto kind : k_Kinds)
	{
		EXPECT_EQ(ValidAsLeashTarget(kind), kind != TieTargetKind::LeashPost && kind != TieTargetKind::SpellIcon);
	}
}

TEST(LeashTie, ADoubleClickOnAThingTiesTheLeashToIt)
{
	EXPECT_EQ(DecideHandTie(OnATree()), HandTie::Tie);
}

TEST(LeashTie, NoTargetOrNoCreatureDoesNothing)
{
	auto check = OnATree();
	check.hasTarget = false;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	check = OnATree();
	check.hasCreature = false;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	// with no creature, even what the leash is tied to does nothing
	check.tied = true;
	check.targetIsTiedObject = true;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
}

TEST(LeashTie, TiedADoubleClickOnTheTiedThingOrTheCreatureUnties)
{
	auto check = OnATree();
	check.tied = true;
	check.targetIsTiedObject = true;
	EXPECT_EQ(DecideHandTie(check), HandTie::Untie);
	check.targetIsTiedObject = false;
	check.targetIsCreature = true;
	EXPECT_EQ(DecideHandTie(check), HandTie::Untie);
	// anything else does nothing, a tree as well as a post
	check.targetIsCreature = false;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	check.kind = TieTargetKind::LeashPost;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
}

TEST(LeashTie, TiedTheUntieNeedsNoLeashInTheHandAndTakesAnyKind)
{
	// the untie is decided before the hand's checks: what it is tied to is untied whatever it is
	for (const auto kind : k_Kinds)
	{
		auto check = OnATree();
		check.kind = kind;
		check.leashInThisHand = false;
		check.tied = true;
		check.targetIsTiedObject = true;
		EXPECT_EQ(DecideHandTie(check), HandTie::Untie);
	}
}

TEST(LeashTie, UntiedTheCreatureItselfIsNoTarget)
{
	auto check = OnATree();
	check.targetIsCreature = true;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
}

TEST(LeashTie, UntiedTheLeashMustBeInThisHand)
{
	auto check = OnATree();
	check.leashInThisHand = false;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	// a seed too: it is tapped only with the leash in the hand
	check.kind = TieTargetKind::OneOffSpellSeed;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
}

TEST(LeashTie, UntiedEachKindOfTarget)
{
	auto check = OnATree();
	check.kind = TieTargetKind::LeashPost;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	check.kind = TieTargetKind::SpellIcon;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	check.kind = TieTargetKind::ScriptHighlight;
	EXPECT_EQ(DecideHandTie(check), HandTie::Nothing);
	check.kind = TieTargetKind::OneOffSpellSeed;
	EXPECT_EQ(DecideHandTie(check), HandTie::Tap);
}

TEST(LeashTie, ATapOnAThingSendsTheCreatureToActOnIt)
{
	EXPECT_EQ(DecideLeashTapOnObject(LeashHeld(), TieTargetKind::Object), LeashTap::ActOnObject);
	// a seed is a thing the leash may be tied to as far as the tap goes
	EXPECT_EQ(DecideLeashTapOnObject(LeashHeld(), TieTargetKind::OneOffSpellSeed), LeashTap::ActOnObject);
}

TEST(LeashTie, ATapOnAThingTheLeashCannotTakeIsTheHandsOwn)
{
	for (const auto kind : {TieTargetKind::LeashPost, TieTargetKind::SpellIcon, TieTargetKind::ScriptHighlight})
	{
		EXPECT_EQ(DecideLeashTapOnObject(LeashHeld(), kind), LeashTap::NotLeash);
	}
}

TEST(LeashTie, ATapOnAThingNeedsTheLeashOnUntiedInThisHand)
{
	auto check = LeashHeld();
	check.leashOnUntied = false;
	EXPECT_EQ(DecideLeashTapOnObject(check, TieTargetKind::Object), LeashTap::NotLeash);
	check = LeashHeld();
	check.hasCreature = false;
	EXPECT_EQ(DecideLeashTapOnObject(check, TieTargetKind::Object), LeashTap::NotLeash);
	check = LeashHeld();
	check.leashInThisHand = false;
	EXPECT_EQ(DecideLeashTapOnObject(check, TieTargetKind::Object), LeashTap::NotLeash);
	check = LeashHeld();
	check.alreadyActingForLeash = true;
	EXPECT_EQ(DecideLeashTapOnObject(check, TieTargetKind::Object), LeashTap::NotLeash);
}

TEST(LeashTie, ATapOnTheLandSendsTheCreatureToActOnThePoint)
{
	EXPECT_EQ(DecideLeashTapOnLand(LeashHeld(), false), LeashTap::ActOnPoint);
	// the leash need not be in this player's hand
	auto check = LeashHeld();
	check.leashInThisHand = false;
	EXPECT_EQ(DecideLeashTapOnLand(check, false), LeashTap::ActOnPoint);
}

TEST(LeashTie, ATapOnTheLandAtTheOriginDoesNothingWhateverTheLeash)
{
	EXPECT_EQ(DecideLeashTapOnLand(LeashHeld(), true), LeashTap::Nothing);
	EXPECT_EQ(DecideLeashTapOnLand(LeashTapCheck {}, true), LeashTap::Nothing);
}

TEST(LeashTie, TheOriginIsZeroBitForBitInItsFirstTwoCoordinatesAndAsANumberInTheThird)
{
	EXPECT_TRUE(IsTapOrigin(glm::vec3(0.0f)));
	EXPECT_TRUE(IsTapOrigin(glm::vec3(0.0f, 0.0f, -0.0f)));
	EXPECT_TRUE(IsTapOrigin(glm::vec3(0.0f, 0.0f, std::numeric_limits<float>::quiet_NaN())));
	EXPECT_FALSE(IsTapOrigin(glm::vec3(-0.0f, 0.0f, 0.0f)));
	EXPECT_FALSE(IsTapOrigin(glm::vec3(0.0f, -0.0f, 0.0f)));
	EXPECT_FALSE(IsTapOrigin(glm::vec3(0.0f, 0.0f, 1.0f)));
	EXPECT_FALSE(IsTapOrigin(glm::vec3(std::numeric_limits<float>::denorm_min(), 0.0f, 0.0f)));
}

TEST(LeashTie, ATapOnTheLandThatIsNotTheLeashs)
{
	auto check = LeashHeld();
	check.leashOnUntied = false;
	EXPECT_EQ(DecideLeashTapOnLand(check, false), LeashTap::NotLeash);
	check = LeashHeld();
	check.hasCreature = false;
	EXPECT_EQ(DecideLeashTapOnLand(check, false), LeashTap::NotLeash);
	check = LeashHeld();
	check.alreadyActingForLeash = true;
	EXPECT_EQ(DecideLeashTapOnLand(check, false), LeashTap::NotLeash);
}
