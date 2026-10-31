/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>

#include <gtest/gtest.h>

#include "Debug/VillagersModel.h"

using namespace openblack;
using namespace openblack::debug::villagers;

TEST(VillagersWindow, NoOneLetGoOfItReadsNone)
{
	EXPECT_EQ(LastLetGoText(std::nullopt), "none");
	EXPECT_EQ(LastLetGoText(PlayerNames::PLAYER_TWO), "player 1");
}

TEST(VillagersWindow, AHitAndAHealAreAnsweredApart)
{
	EXPECT_EQ(ReceiverText(true, true), "a hit: yes, a heal: yes");
	EXPECT_EQ(ReceiverText(false, true), "a hit: no, a heal: yes");
	EXPECT_EQ(YesNo(false), "no");
}

TEST(VillagersWindow, AReactionShowsItsPriorityOnlyWhenItsKindHasOne)
{
	EXPECT_EQ(ReactionText(12, "REACT_TO_BURNING_OBJECT_IN_HAND", 33, uint8_t {90}),
	          "Reaction 12: REACT_TO_BURNING_OBJECT_IN_HAND (33), priority now 90");
	EXPECT_EQ(ReactionText(4, "REACT_TO_FIRE", 2, std::nullopt), "Reaction 4: REACT_TO_FIRE (2), no priority of its own here");
}
