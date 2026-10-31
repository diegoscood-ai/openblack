/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/DiscipleWorkRules.h"

using namespace openblack;
using namespace openblack::disciple_work;

TEST(DiscipleWork, TraderCarriesWhatTheOtherTownWantsMore)
{
	EXPECT_EQ(TradeResource(0.6f, 0.2f), ResourceType::Food);
	EXPECT_EQ(TradeResource(0.2f, 0.6f), ResourceType::Wood);
	// Wanted as much: wood, unless neither is wanted at all
	EXPECT_EQ(TradeResource(0.4f, 0.4f), ResourceType::Wood);
	EXPECT_FALSE(TradeResource(0.0f, 0.0f).has_value());
	// Food wanted a little and wood not at all is still food
	EXPECT_EQ(TradeResource(0.01f, 0.0f), ResourceType::Food);
}

TEST(DiscipleWork, EmptyTraderFetchesFullTraderDelivers)
{
	EXPECT_EQ(ChooseTradeLeg(0, 500, 10.0f, 10.0f), TradeLeg::Fetch);
	EXPECT_EQ(ChooseTradeLeg(500, 500, 10.0f, 10.0f), TradeLeg::Deliver);
	// Half full, equally far: deliver wins ties
	EXPECT_EQ(ChooseTradeLeg(250, 500, 100.0f, 100.0f), TradeLeg::Deliver);
	// Half full but its own store at hand and the other town far off: it fetches
	EXPECT_EQ(ChooseTradeLeg(250, 500, 0.0f, 4000.0f), TradeLeg::Fetch);
	// Empty, both stores beyond the falloff's reach: the falloff never reaches nothing, so it still fetches
	EXPECT_EQ(ChooseTradeLeg(0, 500, 6000.0f, 6000.0f), TradeLeg::Fetch);
}

TEST(DiscipleWork, TraderFetchesItsLimitLessItsLoadUnclamped)
{
	EXPECT_EQ(TradeFetchAmount(500, 0), 500u);
	EXPECT_EQ(TradeFetchAmount(500, 120), 380u);
	EXPECT_EQ(TradeFetchAmount(500, 500), 0u);
	// a load above the limit wraps round, as the game's count does
	EXPECT_EQ(TradeFetchAmount(500, 600), 0xFFFFFF9Cu);
}
