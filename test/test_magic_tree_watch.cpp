/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// How a villager bewildered by a forest miracle goes from tree to tree: the rules of ECS/MagicTreeWatchRules.h, the
// original's arithmetic in its own widths (an unsigned draw range, an 8 bit look count)

#include <cstdint>

#include <gtest/gtest.h>

#include "ECS/MagicTreeWatchRules.h"

using namespace openblack::ecs::magic_tree_watch;

TEST(MagicTreeWatch, TheFirstTreeIsDrawnFromAllButTheLastAndTheTreesGoRound)
{
	EXPECT_EQ(FirstTreeRollRange(18), 17u);
	EXPECT_EQ(FirstTreeRollRange(1), 0u);
	// a forest with no trees left: the range wraps, as the game's unsigned count does
	EXPECT_EQ(FirstTreeRollRange(0), 0xFFFFFFFFu);
	EXPECT_EQ(NextTree(4, 18), 5u);
	EXPECT_EQ(NextTree(17, 18), 0u);
}

TEST(MagicTreeWatch, ItLooksAtTreesUntilAllLessAFreshDrawOfUpToHalf)
{
	EXPECT_EQ(SkippedRollRange(18), 9u);
	EXPECT_EQ(SkippedRollRange(19), 9u);
	EXPECT_EQ(SkippedRollRange(1), 0u);
	EXPECT_TRUE(LooksAtAnother(0, 18, 8));
	EXPECT_TRUE(LooksAtAnother(9, 18, 8));
	EXPECT_FALSE(LooksAtAnother(10, 18, 8));
	EXPECT_TRUE(LooksAtAnother(17, 18, 0));
	EXPECT_FALSE(LooksAtAnother(18, 18, 0));
	EXPECT_TRUE(LooksAtAnother(0, 1, 0));
	EXPECT_FALSE(LooksAtAnother(1, 1, 0));
}

TEST(MagicTreeWatch, ALookIsTheDrawAndTwoSecondsMoreInEightBits)
{
	// ten turns a second: minus two seconds is -20 turns, taken off the draw, so 20 more
	EXPECT_EQ(LookTurns(0, -20), 20);
	EXPECT_EQ(LookTurns(29, -20), 49);
	// the count is 8 bits: a draw that would pass 255 wraps
	EXPECT_EQ(LookTurns(240, -20), 4);
	EXPECT_EQ(LookCounter(20), 0x1414);
}

TEST(MagicTreeWatch, TheLookChangesHalfWayAndCountsDownInEightBits)
{
	auto counter = LookCounter(21);
	EXPECT_FALSE(ChangesLookNow(counter));
	uint32_t turns = 0;
	while (!ChangesLookNow(counter))
	{
		counter = CountDown(counter);
		++turns;
	}
	// 21 halved is 10: the change comes when 10 turns are left
	EXPECT_EQ(turns, 11u);
	EXPECT_EQ(counter & 0xFFu, 10u);
	EXPECT_EQ(counter >> 8u, 21u);
	EXPECT_FALSE(LookOver(counter));
	while (!LookOver(counter))
	{
		counter = CountDown(counter);
	}
	EXPECT_EQ(counter, 0x1500);
	// counted down past 0 the turns left wrap to 255, all the turns kept
	EXPECT_EQ(CountDown(LookCounter(0)), 0x00FF);
}
