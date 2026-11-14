/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "Creature/CreatureDecisionTree.h"
#include "Creature/CreatureFireReaction.h"

using namespace openblack;

TEST(CreatureFire, UsefulnessIsCappedAtAnEighth)
{
	// Combined with compassion it takes more than 0.4 compassion to put a fire out
	EXPECT_FLOAT_EQ(creature_tree::Usefulness(1.0f), 0.125f);
	EXPECT_FALSE(0.125f * 0.4f > creature_fire::k_LeastCompassionateUse);
	EXPECT_TRUE(0.125f * 0.41f > creature_fire::k_LeastCompassionateUse);
}

TEST(CreatureFire, PriorityWithinBothReachesAndBigEnough)
{
	// Within the burning thing's reach and its own, and more than a fifth of the thing's height
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 3.0f, 10.0f, 120), 120);
	// Both tests are strict
	EXPECT_EQ(creature_fire::Priority(10.0f, 5.0f, 5.0f, 3.0f, 10.0f, 120), 0);
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 2.0f, 10.0f, 120), 0);
	EXPECT_EQ(creature_fire::Priority(9.0f, 5.0f, 5.0f, 2.01f, 10.0f, 120), 120);
	// Its own fire: right there, and as tall as itself
	EXPECT_EQ(creature_fire::Priority(0.0f, 4.0f, 4.0f, 30.0f, 30.0f, 120), 120);
	// The radii are added in single precision: 0.1 and 0.4 make exactly 0.5 there, so a thing 0.5 away is not within
	EXPECT_EQ(creature_fire::Priority(0.5f, 0.1f, 0.4f, 3.0f, 10.0f, 120), 0);
}

TEST(CreatureFire, ChoosesToPutOutOnlyWhenItCaresAndKnowsWater)
{
	using creature_fire::Response;
	// Usefulness times compassion must be above a twentieth
	EXPECT_EQ(creature_fire::Choose(0.125f, 0.5f, 3.0f, 3.0f), Response::PutOut);
	EXPECT_EQ(creature_fire::Choose(0.1f, 0.5f, 3.0f, 3.0f), Response::RunAway);
	EXPECT_EQ(creature_fire::Choose(0.125f, 0.4f, 3.0f, 3.0f), Response::RunAway);
	// Nothing the fire belongs to: it runs
	EXPECT_EQ(creature_fire::Choose(std::nullopt, 1.0f, 3.0f, 3.0f), Response::RunAway);
	// Seen the water miracle often enough: exactly enough is enough, more too, less isn't
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 4.0f, 3.0f), Response::PutOut);
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 2.0f, 3.0f), Response::RunAway);
	// Never seen and never needed to: the share is no number, which counts as enough
	EXPECT_EQ(creature_fire::Choose(0.125f, 1.0f, 0.0f, 0.0f), Response::PutOut);
	// No number for how much it cares: it runs
	EXPECT_EQ(creature_fire::Choose(std::numeric_limits<float>::quiet_NaN(), 1.0f, 3.0f, 3.0f), Response::RunAway);
}

TEST(CreatureFire, WhatABurningThingBelongsTo)
{
	using creature_fire::BelongsTo;
	using creature_fire::Burning;
	EXPECT_EQ(creature_fire::Owner(Burning::Building), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Villager), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Field), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::TotemStatue), BelongsTo::Town);
	EXPECT_EQ(creature_fire::Owner(Burning::Tree), BelongsTo::Forest);
	EXPECT_EQ(creature_fire::Owner(Burning::Animal), BelongsTo::Flock);
	EXPECT_EQ(creature_fire::Owner(Burning::Creature), BelongsTo::Itself);
	EXPECT_EQ(creature_fire::Owner(Burning::TemplePart), BelongsTo::Temple);
	EXPECT_EQ(creature_fire::Owner(Burning::Other), BelongsTo::Nothing);
}
