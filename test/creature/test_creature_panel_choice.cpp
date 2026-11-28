/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which creature the status panel shows: the one the hand holds, else the one Creature Mode follows

#include <gtest/gtest.h>

#include "Creature/CreaturePanelChoice.h"

using namespace openblack;
using creature_panel_choice::Choose;
using creature_panel_choice::HiddenByWideScreen;

namespace
{
constexpr auto k_Held = static_cast<entt::entity>(1);
constexpr auto k_Followed = static_cast<entt::entity>(3);
} // namespace

TEST(CreaturePanelChoice, HeldWinsWithTheReward)
{
	const auto choice = Choose(k_Held, k_Followed, 0.25f);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->creature, k_Held);
	ASSERT_TRUE(choice->reward.has_value());
	EXPECT_EQ(*choice->reward, 0.25f);
}

TEST(CreaturePanelChoice, TheHeldOneKeepsItsRewardRowWithNoReward)
{
	// The hand's reward row is drawn whenever the hand holds the creature, at 0 too
	const auto choice = Choose(k_Held, std::nullopt, 0.0f);
	ASSERT_TRUE(choice.has_value());
	ASSERT_TRUE(choice->reward.has_value());
	EXPECT_EQ(*choice->reward, 0.0f);
}

TEST(CreaturePanelChoice, FollowedWithoutTheReward)
{
	const auto choice = Choose(std::nullopt, k_Followed, 0.75f);
	ASSERT_TRUE(choice.has_value());
	EXPECT_EQ(choice->creature, k_Followed);
	EXPECT_FALSE(choice->reward.has_value());
}

TEST(CreaturePanelChoice, NoneWithoutAHeldOrFollowedCreature)
{
	// A creature the hand is only over is not passed: hovering shows no panel
	EXPECT_FALSE(Choose(std::nullopt, std::nullopt, 0.5f).has_value());
}

TEST(CreaturePanelChoice, HiddenOnlyWhileAScriptsWideScreenIsOn)
{
	// on and owned by a script: hidden
	EXPECT_TRUE(HiddenByWideScreen(true, 7));
	// on without an owner, or off: shown
	EXPECT_FALSE(HiddenByWideScreen(true, 0));
	EXPECT_FALSE(HiddenByWideScreen(false, 0));
	EXPECT_FALSE(HiddenByWideScreen(false, 7));
}
