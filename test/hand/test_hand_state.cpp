/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's state, chosen every frame (hand_state::Required)

#include <gtest/gtest.h>

#include "Hand/HandStateRules.h"

using namespace openblack;
namespace hs = openblack::hand_state;

TEST(HandState, InsideTheTempleItIsAlwaysTheTempleState)
{
	EXPECT_EQ(hs::Required({.insideCitadel = true}), hs::k_Citadel);
	EXPECT_EQ(hs::Required({.insideCitadel = true, .interfaceState = 0x19, .holding = true, .holdsSeed = true}), hs::k_Citadel);
}

TEST(HandState, WhatIsHeldPicksTheStateAPileInTheLockedSelectFirst)
{
	EXPECT_EQ(hs::Required({.holding = true, .holdsSeed = true}), hs::k_Grain);
	EXPECT_EQ(hs::Required({.holding = true, .tugging = true}), hs::k_Tug);
	EXPECT_EQ(hs::Required({.holding = true}), hs::k_Holding);
	// a pile in the locked select is holding, before the seed and the tug
	EXPECT_EQ(hs::Required({.holding = true, .lockedSelectOnPile = true, .holdsSeed = true, .tugging = true}), hs::k_Holding);
	// held things come before the interface's own states and a set animation
	EXPECT_EQ(hs::Required({.interfaceState = 0xE, .holding = true, .playingAnimation = true}), hs::k_Holding);
}

TEST(HandState, TheInterfacesModeHidesTheHandEvenHoldingInItsMode0x19)
{
	EXPECT_EQ(hs::Required({.interfaceState = 0x19, .holding = true}), hs::k_Invisible);
	for (const int32_t mode : {1, 2, 0xC, 0x19})
	{
		EXPECT_EQ(hs::Required({.interfaceState = mode}), hs::k_Invisible) << mode;
	}
	// holding in another hidden mode keeps the held state
	EXPECT_EQ(hs::Required({.interfaceState = 1, .holding = true}), hs::k_Holding);
}

TEST(HandState, ASetAnimationComesBeforeTheInterfacesStates)
{
	EXPECT_EQ(hs::Required({.interfaceState = 1, .playingAnimation = true}), hs::k_PlayAnim);
	EXPECT_EQ(hs::Required({.playingAnimation = true}), hs::k_PlayAnim);
}

TEST(HandState, TheTotemAndTheCreatureStatesElseNormal)
{
	EXPECT_EQ(hs::Required({.interfaceState = 0xE}), hs::k_Totem);
	EXPECT_EQ(hs::Required({.interfaceState = 0x13, .creatureInHand = true}), hs::k_Creature);
	EXPECT_EQ(hs::Required({.interfaceState = 0x13}), hs::k_Normal);
	EXPECT_EQ(hs::Required({}), hs::k_Normal);
	EXPECT_EQ(hs::Required({.interfaceState = 7}), hs::k_Normal);
}
