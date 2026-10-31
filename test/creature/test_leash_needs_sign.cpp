/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A leash tied to a totem's needs sign (Creature/LeashNeedsSign.h): which signs qualify, how full a sign is, when the
// creature is set to help, and what it does

#include <cmath>

#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/LeashNeedsSign.h"

using namespace openblack;
using namespace openblack::creature_leash;

TEST(LeashNeedsSign, OnlyATotemsSignsAtAWorshipSiteQualify)
{
	for (const auto row : {NeedsSignRow::Food, NeedsSignRow::Rest, NeedsSignRow::PeopleHiding})
	{
		EXPECT_TRUE(NeedsSignQualifies({.row = row, .totemHasWorshipSite = true}));
		EXPECT_FALSE(NeedsSignQualifies({.row = row, .totemHasWorshipSite = false}));
	}
	EXPECT_FALSE(NeedsSignQualifies({.row = NeedsSignRow::Workshop, .totemHasWorshipSite = true}));
}

TEST(LeashNeedsSign, TheFillIsTheDesireOverTheMostAtMostOne)
{
	EXPECT_EQ(NeedsSignFill(0.0f, 4.0f), 0.0f);
	EXPECT_EQ(NeedsSignFill(1.0f, 4.0f), 0.25f);
	EXPECT_EQ(NeedsSignFill(3.0f, 5.0f), 3.0f / 5.0f);
	EXPECT_EQ(NeedsSignFill(4.0f, 4.0f), 1.0f);
	EXPECT_EQ(NeedsSignFill(9.0f, 4.0f), 1.0f);
	// with no most it can show, the sign can't say how full it is; a desire that can't be compared shows the most
	EXPECT_TRUE(std::isnan(NeedsSignFill(1.0f, 0.0f)));
	EXPECT_EQ(NeedsSignFill(std::numeric_limits<float>::quiet_NaN(), 4.0f), 1.0f);
}

TEST(LeashNeedsSign, TheTieArmsItHoweverFullTheSignIs)
{
	EXPECT_TRUE(NeedsSignArmsAtTie({.row = NeedsSignRow::Rest, .totemHasWorshipSite = true}));
	EXPECT_FALSE(NeedsSignArmsAtTie({.row = NeedsSignRow::Workshop, .totemHasWorshipSite = true}));
	EXPECT_FALSE(NeedsSignArmsAtTie({.row = NeedsSignRow::Food, .totemHasWorshipSite = false}));
}

TEST(LeashNeedsSign, EachTurnItIsArmedAgainOnlyAboveSixTenths)
{
	const NeedsSign food {.row = NeedsSignRow::Food, .totemHasWorshipSite = true};
	EXPECT_TRUE(NeedsSignArmsThisTurn(food, 1.0f));
	EXPECT_TRUE(NeedsSignArmsThisTurn(food, 0.61f));
	// 0.6 as a float is a little more than 0.6: compared as a double, it counts
	EXPECT_TRUE(NeedsSignArmsThisTurn(food, 0.6f));
	EXPECT_FALSE(NeedsSignArmsThisTurn(food, std::nextafter(0.6f, 0.0f)));
	EXPECT_FALSE(NeedsSignArmsThisTurn(food, 0.0f));
	EXPECT_FALSE(NeedsSignArmsThisTurn(food, std::numeric_limits<float>::quiet_NaN()));
	EXPECT_FALSE(NeedsSignArmsThisTurn({.row = NeedsSignRow::Workshop, .totemHasWorshipSite = true}, 1.0f));
}

TEST(LeashNeedsSign, TheFoodMiracleBeforeFishing)
{
	EXPECT_EQ(NeedsSignAction(true, true), std::optional<uint32_t>(k_FoodForWorshipAction));
	EXPECT_EQ(NeedsSignAction(true, false), std::optional<uint32_t>(k_FoodForWorshipAction));
	EXPECT_EQ(NeedsSignAction(false, true), std::optional<uint32_t>(k_FishForWorshipAction));
	EXPECT_EQ(NeedsSignAction(false, false), std::nullopt);
	EXPECT_EQ(k_FoodForWorshipAction, 246u);
	EXPECT_EQ(k_FishForWorshipAction, 300u);
}
