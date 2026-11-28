/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's state, chosen every frame (hand_state::Required)

#include <array>

#include <gtest/gtest.h>

#include "3D/HandCrossFade.h"
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

TEST(HandState, TheCameraStateWhileThePlayersCameraGripsTheLandInTheInterfacesOtherModes)
{
	EXPECT_EQ(hs::Required({.cameraGripsLand = true}), hs::k_Camera);
	// the interface's land grip (its mode 20) and any mode of its own
	EXPECT_EQ(hs::Required({.interfaceState = 20, .cameraGripsLand = true}), hs::k_Camera);
	EXPECT_EQ(hs::Required({.interfaceState = 20}), hs::k_Normal);
	// not over the creature's mode, even with no creature locked, nor the totem's, the hidden modes, a set animation,
	// what is held or the temple
	EXPECT_EQ(hs::Required({.interfaceState = 0x13, .cameraGripsLand = true}), hs::k_Normal);
	EXPECT_EQ(hs::Required({.interfaceState = 0xE, .cameraGripsLand = true}), hs::k_Totem);
	for (const int32_t mode : {1, 2, 0xC, 0x19})
	{
		EXPECT_EQ(hs::Required({.interfaceState = mode, .cameraGripsLand = true}), hs::k_Invisible) << mode;
	}
	EXPECT_EQ(hs::Required({.playingAnimation = true, .cameraGripsLand = true}), hs::k_PlayAnim);
	EXPECT_EQ(hs::Required({.holding = true, .cameraGripsLand = true}), hs::k_Holding);
	EXPECT_EQ(hs::Required({.insideCitadel = true, .cameraGripsLand = true}), hs::k_Citadel);
}

// The hand fades its pose over 0.13 s each time its state changes, and only then: taking a seed (the grain state) and
// losing it (back to normal) fade, while the seed becoming ready or powering up leaves what the state reads (held, a
// seed) as it was, so the state stays and nothing fades (docs/wiki/magic/hand-casting.md)
TEST(HandState, TakingOrLosingASeedFadesThePoseButItsReadinessAndPowerUpDoNot)
{
	constexpr hs::RequiredStateFacts k_Empty {};
	constexpr hs::RequiredStateFacts k_HoldingASeed {.holding = true, .holdsSeed = true};
	struct Frame
	{
		hs::RequiredStateFacts facts;
		bool fades;
	};
	// the frames of a seed's life in the hand: the seed is taken, becomes ready, powers up twice and is lost, each once
	// the last one's fade is over
	const std::array k_Frames = {
	    Frame {k_Empty, false},        // the empty hand
	    Frame {k_HoldingASeed, true},  // the seed taken
	    Frame {k_HoldingASeed, false}, // the seed ready
	    Frame {k_HoldingASeed, false}, // the first power-up
	    Frame {k_HoldingASeed, false}, // the second
	    Frame {k_Empty, true},         // the seed lost
	};
	int32_t state = hs::Required(k_Empty);
	HandCrossFade fade;
	for (const auto& [facts, fades] : k_Frames)
	{
		// a change of the state starts the fade, which then runs 0.13 s, as the hand system takes the state up
		if (hs::TakeUp(state, hs::Required(facts)))
		{
			fade.Start({0.0f, 0.0f, 0.0f});
		}
		EXPECT_EQ(fade.IsActive(), fades);
		int frames = 0;
		for (; fade.IsActive(); ++frames)
		{
			fade.Update(1.0f / 30.0f);
		}
		// 0.13 s at 1/30 s a frame: over on the fourth frame
		EXPECT_EQ(frames, fades ? 4 : 0);
	}
	EXPECT_EQ(hs::Required(k_HoldingASeed), hs::k_Grain);
	EXPECT_EQ(state, hs::k_Normal);
}
