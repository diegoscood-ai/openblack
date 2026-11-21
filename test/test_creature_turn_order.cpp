/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <array>
#include <initializer_list>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureTurnOrder.h"

using namespace openblack;
using creature_desires::Desire;
namespace turn = openblack::creature_turn_order;

namespace
{
std::array<bool, creature_desires::k_DesireCount> BodyOnly(std::initializer_list<Desire> desires)
{
	std::array<bool, creature_desires::k_DesireCount> body {};
	for (const auto desire : desires)
	{
		body.at(static_cast<size_t>(desire)) = true;
	}
	return body;
}

creature_desires::Desires AllAt(float value)
{
	creature_desires::Desires desires;
	for (auto& desire : desires.desires)
	{
		desire.value = value;
	}
	return desires;
}
} // namespace

TEST(CreatureTurnOrder, BodyDesiresStayAtZeroBeforeTheFirstStage)
{
	auto desires = AllAt(0.7f);
	turn::HoldBodyDesires(desires, BodyOnly({Desire::Fear, Desire::Tiredness, Desire::Rest}), 0);
	EXPECT_EQ(desires[Desire::Fear].value, 0.0f);
	EXPECT_EQ(desires[Desire::Tiredness].value, 0.0f);
	EXPECT_EQ(desires[Desire::Rest].value, 0.0f);
	EXPECT_EQ(desires[Desire::Curiosity].value, 0.7f);
}

TEST(CreatureTurnOrder, BodyDesiresGrowFromTheFirstStage)
{
	auto desires = AllAt(0.7f);
	turn::HoldBodyDesires(desires, BodyOnly({Desire::Fear}), 1);
	EXPECT_EQ(desires[Desire::Fear].value, 0.7f);
}

TEST(CreatureTurnOrder, APlanStartsAgainOnlyAfterAHoldAndWithStepsToDo)
{
	EXPECT_TRUE(turn::RestartsAfterHold(true, true));
	EXPECT_FALSE(turn::RestartsAfterHold(true, false));
	EXPECT_FALSE(turn::RestartsAfterHold(false, true));
}

TEST(CreatureTurnOrder, FollowingThePlayerDoesNotSetItOff)
{
	EXPECT_TRUE(turn::SetsOff(true, Desire::Hunger));
	EXPECT_FALSE(turn::SetsOff(true, Desire::FollowPlayerDesire));
	EXPECT_FALSE(turn::SetsOff(false, Desire::Hunger));
}

TEST(CreatureTurnOrder, StuckOnTheTurnAfterFifteenSecondsOfNotSettingOff)
{
	constexpr uint32_t k_TurnsPerSecond = 10;
	uint32_t turns = 0;
	for (uint32_t i = 0; i < 150; ++i)
	{
		EXPECT_FALSE(turn::StepStuck(turns, false, k_TurnsPerSecond));
	}
	EXPECT_EQ(turns, 150u);
	EXPECT_TRUE(turn::StepStuck(turns, false, k_TurnsPerSecond));
	EXPECT_EQ(turns, 0u);
}

TEST(CreatureTurnOrder, TheStuckCountKeepsItsValueOnceItHasSetOff)
{
	uint32_t turns = 40;
	EXPECT_FALSE(turn::StepStuck(turns, true, 10));
	EXPECT_EQ(turns, 40u);
	// a count already past the limit times out even though it has set off since
	turns = 151;
	EXPECT_TRUE(turn::StepStuck(turns, true, 10));
	EXPECT_EQ(turns, 0u);
}
