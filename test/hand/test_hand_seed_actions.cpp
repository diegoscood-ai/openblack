/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand with a spell seed in it, through the hand's own presses and releases (support/HandSeedHarness.h)

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Components/Unavailable.h"
#include "support/HandSeedHarness.h"

using namespace openblack;
using openblack::test::hand_seed::k_GestureLoop;
using HandResult = openblack::ecs::systems::MagicSystemInterface::HandResult;

namespace
{
using HandSeedActions = openblack::test::hand_seed::HandSeedHarness;
} // namespace

// A gesture seed is armed by the press and waits for the release; a second press before it is no press, as the button
// never came up
TEST_F(HandSeedActions, ASecondPressWhileAGestureSeedIsArmedIsIgnored)
{
	const auto seed = HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().GetHeldObject(), std::optional(seed));
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	EXPECT_EQ(Hand().PressSeedAction(), HandResult::None);
	EXPECT_EQ(Tags().made.size(), 1u); // armed once
	EXPECT_EQ(Hand().GetHeldObject(), std::optional(seed));
}

// While armed the hand loops the gesture sample at itself, 3D, in the game's bank; the release ends the loop, which
// plays out its pass
TEST_F(HandSeedActions, AnArmedGestureSeedHumsAtTheHandUntilTheReleaseLetsTheLoopPlayOut)
{
	HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	ASSERT_EQ(Tags().made.size(), 1u);
	const auto& hum = Tags().made.front();
	EXPECT_EQ(hum.sample, k_GestureLoop);
	EXPECT_FALSE(hum.track);
	EXPECT_EQ(hum.mode, 2);
	EXPECT_EQ(hum.loops, -1);
	EXPECT_TRUE(hum.is3D);
	EXPECT_EQ(hum.bank, audio::SfxBank::InGame);
	EXPECT_TRUE(Tags().removed.empty());
	static_cast<void>(Hand().ReleaseSeedAction());
	ASSERT_EQ(Tags().removed.size(), 1u);
	EXPECT_EQ(Tags().removed.front().sample, k_GestureLoop);
	EXPECT_EQ(Tags().removed.front().bank, audio::SfxBank::InGame);
	EXPECT_FALSE(Tags().removed.front().stop);
}

// A seed that leaves the hand while armed (gone, or dropped) ends the loop the same way at the next step
TEST_F(HandSeedActions, AnArmedSeedThatLeavesTheHandLetsTheLoopPlayOut)
{
	const auto seed = HoldSeed(SpellSeedType::Storm, SpellCastType::SpellCastHandGesture);
	ASSERT_EQ(Hand().PressSeedAction(), HandResult::Readied);
	Registry().Assign<ecs::components::Unavailable>(seed);
	EXPECT_EQ(Hand().ReleaseSeedAction(), HandResult::None);
	ASSERT_EQ(Tags().removed.size(), 1u);
	EXPECT_EQ(Tags().removed.front().sample, k_GestureLoop);
	EXPECT_FALSE(Tags().removed.front().stop);
}
