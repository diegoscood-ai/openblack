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
	// Held in a hand's locked select, wearing a leash that does not work, or of a player the computer plays, it copies
	// nothing
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, {.phase = 13, .lockedSelect = true}));
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, {.phase = 13, .leashWorks = false}));
	EXPECT_FALSE(MayStartMimicry(mimicry, 1, rules, {.phase = 13, .computerPlayer = true}));
	ASSERT_TRUE(StartMimicry(mimicry, 1, rules, grown, 7u, 0, [](uint32_t) { return 0u; }));
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

// The noticing begins at the turn the deed is seen, with nothing finished, its limit the table's count plus a draw of
// GameRand(1), as the original's start of copying a seen deed (docs/wiki/creature/mimicry.md#the-stages-of-copying)
TEST(CreatureCopyActions, BeginStartsWithNoTestItsLimitTheTablesCountAndADrawOfOne)
{
	const auto rules = Rules();
	std::optional<Mimicry> mimicry;
	std::vector<uint32_t> ranges;
	const auto random = [&ranges](uint32_t range) {
		ranges.push_back(range);
		return 0u;
	};
	BeginMimicry(mimicry, 1, rules, 9u, 500, random);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::Notice);
	EXPECT_EQ(mimicry->progress, 0u);
	EXPECT_EQ(mimicry->limit, 3u);
	EXPECT_EQ(mimicry->stageStarted, 500u);
	EXPECT_EQ(mimicry->object, 9u);
	EXPECT_EQ(ranges, std::vector<uint32_t> {1u});
	// a row out of range starts nothing and draws nothing
	BeginMimicry(mimicry, 4, rules, std::nullopt, 600, random);
	EXPECT_EQ(mimicry->rule, 1u);
	EXPECT_EQ(ranges.size(), 1u);
}

// Choosing to copy the player again starts on the last deed afresh, its limit drawn as a new start draws it, then goes
// straight to copying the deed with half the limit counted, as the original's agenda for copying the player
// (docs/wiki/creature/mimicry.md#the-stages-of-copying)
TEST(CreatureCopyActions, CopyingAgainGoesStraightToTheDeedWithHalfItsLimitCounted)
{
	const auto rules = Rules();
	std::vector<uint32_t> ranges;
	const auto random = [&ranges](uint32_t range) {
		ranges.push_back(range);
		return 0u;
	};
	std::optional<Mimicry> mimicry = Mimicry {.rule = 0, .stage = MimicStage::CopyDesire, .progress = 7, .limit = 9};
	CopyDeedAgain(mimicry, 1, rules, 9u, 700, random);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->rule, 1u);
	EXPECT_EQ(mimicry->stage, MimicStage::CopyAction);
	// the table's 3 and a draw of 0: half of 3 counted, in whole actions
	EXPECT_EQ(mimicry->limit, 3u);
	EXPECT_EQ(mimicry->progress, 1u);
	EXPECT_EQ(mimicry->stageStarted, 700u);
	EXPECT_EQ(mimicry->object, 9u);
	EXPECT_EQ(ranges, std::vector<uint32_t> {1u});
	// a row out of range copies nothing and draws nothing
	CopyDeedAgain(mimicry, 4, rules, std::nullopt, 800, random);
	EXPECT_EQ(mimicry->rule, 1u);
	EXPECT_EQ(mimicry->stageStarted, 700u);
	EXPECT_EQ(ranges.size(), 1u);
}

// Each stage is counted in actions finished, not in time: noticing ends after one, copying the action and the desire
// after the limit, any stage after more than its time (180 s, 1800 turns at ten a second); the next stage draws its limit as it
// begins, and only a deed that copies the desire has that stage (docs/wiki/creature/mimicry.md#the-stages-of-copying)
TEST(CreatureCopyActions, TheStagesEndByTheActionsFinishedOrTheirTime)
{
	std::vector<MimicRule> rules = Rules();
	rules.at(0).copiesDesire = true;
	std::vector<uint32_t> ranges;
	const auto random = [&ranges](uint32_t range) {
		ranges.push_back(range);
		return 0u;
	};
	std::optional<Mimicry> mimicry;
	BeginMimicry(mimicry, 0, rules, 9u, 100, random);
	// noticing lasts while no action is finished, up to its time
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 100, 1800, random), MimicStage::Notice);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 1900, 1800, random), MimicStage::Notice);
	CountFinishedAction(mimicry);
	// one finished, the pass moves on and forces nothing
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2000, 1800, random), std::nullopt);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::CopyAction);
	EXPECT_EQ(mimicry->progress, 0u);
	EXPECT_EQ(mimicry->limit, 2u);
	EXPECT_EQ(mimicry->stageStarted, 2000u);
	EXPECT_EQ(ranges, (std::vector<uint32_t> {1u, 1u}));
	// copying the action lasts until the table's two actions are finished
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2001, 1800, random), MimicStage::CopyAction);
	CountFinishedAction(mimicry);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2002, 1800, random), MimicStage::CopyAction);
	CountFinishedAction(mimicry);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2003, 1800, random), std::nullopt);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::CopyDesire);
	EXPECT_EQ(mimicry->stageStarted, 2003u);
	// copying the desire, with nothing finished, lasts its 1800 turns and ends on the next
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2003 + 1800, 1800, random), MimicStage::CopyDesire);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 2003 + 1801, 1800, random), std::nullopt);
	EXPECT_FALSE(mimicry.has_value());
	EXPECT_EQ(ranges.size(), 3u);

	// a deed that does not copy the desire stops after copying the action, with no draw
	BeginMimicry(mimicry, 1, rules, std::nullopt, 0, random);
	CountFinishedAction(mimicry);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 1, 1800, random), std::nullopt);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::CopyAction);
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 1802, 1800, random), std::nullopt);
	EXPECT_FALSE(mimicry.has_value());
	EXPECT_EQ(ranges.size(), 5u);

	// the draw is added to the limit; a limit of none ends a stage at its first pass
	rules.at(2).stageSteps = 0;
	BeginMimicry(mimicry, 2, rules, std::nullopt, 0, [](uint32_t) { return 0u; });
	EXPECT_EQ(ProcessMimicry(mimicry, rules, 0, 1800, random), std::nullopt);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::CopyAction);
	BeginMimicry(mimicry, 1, rules, std::nullopt, 0, [](uint32_t) { return 2u; });
	EXPECT_EQ(mimicry->limit, 5u);

	// finished actions count only while it copies
	std::optional<Mimicry> none;
	CountFinishedAction(none);
	EXPECT_FALSE(none.has_value());
	EXPECT_EQ(ProcessMimicry(none, rules, 0, 1800, random), std::nullopt);
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

TEST(CreatureCopyActions, StrokedWhileCopyingItDoesTheThingItselfForTheStagesStepsAndTheDraw)
{
	const auto rules = Rules();
	std::optional<Mimicry> mimicry =
	    Mimicry {.rule = 1, .stage = MimicStage::Notice, .progress = 1, .limit = 1, .stageStarted = 40};
	// the game draws GameRand(1), always none, and adds it to the table's count; the actions finished start again and
	// the stage keeps the turn it began at
	StrokedWhileMimicking(mimicry, rules, 0);
	ASSERT_TRUE(mimicry.has_value());
	EXPECT_EQ(mimicry->stage, MimicStage::CopyAction);
	EXPECT_EQ(mimicry->progress, 0u);
	EXPECT_EQ(mimicry->limit, 3u);
	EXPECT_EQ(mimicry->stageStarted, 40u);
	StrokedWhileMimicking(mimicry, rules, 2);
	EXPECT_EQ(mimicry->limit, 5u);
	// not copying: nothing
	std::optional<Mimicry> none;
	StrokedWhileMimicking(none, rules, 0);
	EXPECT_FALSE(none.has_value());
}
