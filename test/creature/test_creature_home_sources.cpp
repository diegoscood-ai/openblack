/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <limits>
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

TEST(CreatureDesireSources, TheWishToShowHowItFeelsIsTheSumTimesAThird)
{
	// The game multiplies by the float a third: for a sum of 0.005 that is one bit above the sum divided by three
	EXPECT_EQ(creature_desires::ManifestStateSource(0.005f), 0.005f * (1.0f / 3.0f));
	EXPECT_NE(creature_desires::ManifestStateSource(0.005f), 0.005f / 3.0f);
	EXPECT_FLOAT_EQ(creature_desires::ManifestStateSource(1.5f), 0.5f);
	EXPECT_EQ(creature_desires::ManifestStateSource(4.0f), 1.0f);
	EXPECT_EQ(creature_desires::ManifestStateSource(-1.0f), 0.0f);
	// The low end is tested first: a sum that is not a number gives 0
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_desires::ManifestStateSource(std::numeric_limits<float>::quiet_NaN())), 0u);
}

TEST(CreatureDesireSources, TheUrgeToGetHighIsTheMushroomsEatenInWholeHundreds)
{
	// the game divides the count by a hundred as whole numbers: none below a hundred, then steps of one
	EXPECT_EQ(creature_desires::GetHighSource(0), 0.0f);
	EXPECT_EQ(creature_desires::GetHighSource(99), 0.0f);
	EXPECT_EQ(creature_desires::GetHighSource(100), 1.0f);
	EXPECT_EQ(creature_desires::GetHighSource(250), 2.0f);
	// the mental illness desire grows from the same count, the game's source after the one of being near home
	EXPECT_EQ(creature_desires::sources::k_Craziness, creature_desires::sources::k_HangAroundAtHome + 1);
}
