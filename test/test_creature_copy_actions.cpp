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
#include <vector>

#include <gtest/gtest.h>

#include "Creature/CreatureLook.h"
#include "Creature/CreatureWatching.h"

using namespace openblack;
using namespace openblack::creature_watching;

namespace
{
std::vector<MimicRule> Rules()
{
	return {
	    {.name = "Food", .chance = 0.4f, .needsLearningLeash = false, .copyActions = {245, 300}, .stageSteps = 2},
	    {.name = "Rocks", .chance = 0.4f, .needsLearningLeash = false, .copyActions = {266}, .stageSteps = 3},
	    {.name = "Low", .chance = 0.1f, .needsLearningLeash = false, .stageSteps = 1},
	};
}
} // namespace

TEST(CreatureCopyActions, MayStartIsTheTestOfStart)
{
	const auto rules = Rules();
	const MimicConditions grown {.phase = 13};
	std::optional<Mimicry> mimicry;
	EXPECT_TRUE(MayStartMimicry(mimicry, 1, rules, grown));
	EXPECT_FALSE(mimicry.has_value());
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, {.phase = 2}));
	EXPECT_FALSE(MayStartMimicry(mimicry, 5, rules, grown));
	ASSERT_TRUE(StartMimicry(mimicry, 1, rules, grown, 7u));
	// The same deed again does not start again
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, grown));
	// A deed of the same rank starts over it, one ranked lower does not
	EXPECT_TRUE(MayStartMimicry(mimicry, 0, rules, grown));
	EXPECT_FALSE(MayStartMimicry(mimicry, 2, rules, grown));
}

TEST(CreatureCopyActions, OnceItCopiesTheDesireAnyDeedStartsOverIt)
{
	const auto rules = Rules();
	const MimicConditions grown {.phase = 13};
	std::optional<Mimicry> mimicry = Mimicry {.rule = 1, .stage = MimicStage::CopyDesire};
	EXPECT_TRUE(MayStartMimicry(mimicry, 1, rules, grown));
	EXPECT_TRUE(MayStartMimicry(mimicry, 2, rules, grown));
	// Its state still has to allow it
	EXPECT_FALSE(MayStartMimicry(mimicry, 2, rules, {.phase = 13, .reactionPriority = 150.0f}));
	EXPECT_FALSE(MayStartMimicry(mimicry, 2, rules, {.phase = 13, .canSee = false}));
}

TEST(CreatureCopyActions, BeginStartsWithNoTest)
{
	const auto rules = Rules();
	std::optional<Mimicry> mimicry;
	BeginMimicry(mimicry, 1, rules, 9u);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::Notice);
	EXPECT_EQ(mimicry->stepsLeft, 3u);
	EXPECT_EQ(mimicry->object, 9u);
	BeginMimicry(mimicry, 4, rules, std::nullopt);
	EXPECT_EQ(mimicry->rule, 1u);
}

TEST(CreatureCopyActions, CastDeedGateCountsWhatIsStillNeeded)
{
	// Ten sightings needed at a multiplier of 1
	EXPECT_EQ(CastDeedGate(0, 10, 1.0f), CastDeedStep::CountAndStop);
	EXPECT_EQ(CastDeedGate(8, 10, 1.0f), CastDeedStep::CountAndStop);
	EXPECT_EQ(CastDeedGate(9, 10, 1.0f), CastDeedStep::CountThenRoll);
	EXPECT_EQ(CastDeedGate(10, 10, 1.0f), CastDeedStep::Copy);
	EXPECT_EQ(CastDeedGate(30, 10, 1.0f), CastDeedStep::Copy);
	// The still needed count is cut towards zero: 1.5 still needed is one
	EXPECT_EQ(CastDeedGate(5, 10, 0.65f), CastDeedStep::CountThenRoll);
	// 0.9 still needed is none
	EXPECT_EQ(CastDeedGate(6, 10, 0.69f), CastDeedStep::Copy);
	// The times needed are a float: 10 x 0.7f rounds to exactly 7, so with 5 seen 2 are still needed (the oracle's
	// value; at full precision it would be just under 7)
	EXPECT_EQ(CastDeedGate(5, 10, 0.7f), CastDeedStep::CountAndStop);
	// The same at the other boundaries the oracle gave: 13, 7, 19 and 9 exactly
	EXPECT_EQ(CastDeedGate(11, 10, 1.3f), CastDeedStep::CountAndStop);
	EXPECT_EQ(CastDeedGate(6, 5, 1.4f), CastDeedStep::CountThenRoll);
	EXPECT_EQ(CastDeedGate(18, 10, 1.9f), CastDeedStep::CountThenRoll);
	EXPECT_EQ(CastDeedGate(9, 10, 0.9f), CastDeedStep::Copy);
}

TEST(CreatureCopyActions, ADeedIsCopiedOnlyNearerInMetresThanTheCellsTheEyesSweep)
{
	// at size 1 the eyes sweep a square of 15 cells a side
	EXPECT_EQ(creature_look::SweepCells(1.0f), 225u);
	EXPECT_TRUE(WithinCopyReach(224.9f, creature_look::SweepCells(1.0f)));
	EXPECT_FALSE(WithinCopyReach(225.0f, creature_look::SweepCells(1.0f)));
	// a bigger creature sweeps more
	EXPECT_GT(creature_look::SweepCells(4.0f), creature_look::SweepCells(1.0f));
}
