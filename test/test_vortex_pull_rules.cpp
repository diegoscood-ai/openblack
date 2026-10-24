/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which creature a vortex out of the land takes (Magic/VortexPullRules.h; raffclar's cases, PR #349)

#include <gtest/gtest.h>

#include "Magic/VortexPullRules.h"

using namespace openblack;

TEST(VortexPull, OnlyACreatureLetThroughOnTheLeashIsTaken)
{
	EXPECT_TRUE(vortex::CreatureMayBeTaken(true, true, false));
	EXPECT_FALSE(vortex::CreatureMayBeTaken(true, false, false));
	EXPECT_FALSE(vortex::CreatureMayBeTaken(false, true, false));
	// or one a script controls
	EXPECT_TRUE(vortex::CreatureMayBeTaken(false, false, true));
}

TEST(VortexPull, ACreatureSparklesAwayForGoodOverThreeSeconds)
{
	EXPECT_EQ(vortex::k_CreatureTakenShare, 0.7f);
	EXPECT_EQ(vortex::k_CreatureSparkleSeconds, 3.0f);
	EXPECT_EQ(vortex::k_CreatureSparkleTarget, 1.0f);
}

TEST(VortexPull, OnlyAnOpenWayOutTakesWhatStrikesItInFlight)
{
	using openblack::VortexStateType;
	using openblack::VortexType;
	EXPECT_TRUE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, true, true));
	// not while it fades in or out
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::FadeIn, true, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::FadeOut, true, true));
	// never a way into a land nor the volcano
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::Out, VortexStateType::Active, true, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::Volcano, VortexStateType::Active, true, true));
	// only what still flies and could be drawn in from the land
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, false, true));
	EXPECT_FALSE(vortex::TakesWhatStrikesIt(VortexType::In, VortexStateType::Active, true, false));
}
