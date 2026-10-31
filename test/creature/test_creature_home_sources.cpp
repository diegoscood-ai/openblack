/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"

using namespace openblack;
using creature_desires::HomeSource;
namespace sources = creature_desires::sources;

TEST(CreatureDesireSources, GoingHomeFollowsTheDistanceFromHome)
{
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_GoHome, 50.0f, 200.0f), 0.25f);
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_GoHome, 500.0f, 200.0f), 1.0f);
	// No distance for the stage: away from home it wants to go home fully, at home not at all
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_GoHome, 500.0f, 0.0f), 1.0f);
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_GoHome, 0.0f, 0.0f), 0.0f);
	// Without a home the source keeps its own value
	EXPECT_FALSE(HomeSource(sources::k_GoHome, std::nullopt, 200.0f).has_value());
}

TEST(CreatureDesireSources, HangingAroundAtHomeFallsToNothingAHundredAway)
{
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_HangAroundAtHome, 50.0f, 200.0f), 0.5f);
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_HangAroundAtHome, 500.0f, 200.0f), 0.0f);
	EXPECT_FLOAT_EQ(*HomeSource(sources::k_HangAroundAtHome, 0.0f, 200.0f), 1.0f);
	// Without a home the source keeps its own value, and no other source is read from the home
	EXPECT_FALSE(HomeSource(sources::k_HangAroundAtHome, std::nullopt, 200.0f).has_value());
	EXPECT_FALSE(HomeSource(sources::k_Sadness, 50.0f, 200.0f).has_value());
}
