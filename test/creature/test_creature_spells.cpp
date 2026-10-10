/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>
#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureDesires.h"
#include "Creature/CreatureSpells.h"

using namespace openblack;
using namespace openblack::creature_spells;

namespace
{
constexpr float k_TurnsPerSecond = 10.0f;
constexpr float k_Epsilon = 1e-4f;
/// The 2 s a spell waits before it starts, at 10 turns a second
constexpr int k_DelayTurns = 20;

/// The game's times: every spell holds 25 seconds; freeze eases in and out over 2, the body spells 4, invisible 5
std::array<Timing, k_SpellCount> Timings()
{
	std::array<Timing, k_SpellCount> timings {};
	timings.fill({.startSeconds = 4.0f, .finishSeconds = 4.0f});
	timings.at(static_cast<size_t>(Spell::Freeze)) = {.startSeconds = 2.0f, .finishSeconds = 2.0f};
	timings.at(static_cast<size_t>(Spell::Invisible)) = {.startSeconds = 5.0f, .finishSeconds = 5.0f};
	timings.at(static_cast<size_t>(Spell::Nice)) = {.startSeconds = 3.0f, .finishSeconds = 3.0f};
	timings.at(static_cast<size_t>(Spell::Itchy)) = {.startSeconds = 1.0f, .finishSeconds = 1.0f};
	return timings;
}

const auto k_Miracle = static_cast<entt::entity>(42);
const auto k_Other = static_cast<entt::entity>(43);

/// Steps until a spell is off, counting the turns and keeping every event
struct Run
{
	int turns {0};
	std::vector<TurnEvent> events;
	std::vector<entt::entity> ended;
};
Run StepUntilOff(Spells& spells, Spell spell, int limit = 2000)
{
	const auto timings = Timings();
	Run run;
	while (spells.IsActive(spell) && run.turns < limit)
	{
		auto turn = Step(spells, timings, k_TurnsPerSecond);
		run.events.insert(run.events.end(), turn.events.begin(), turn.events.end());
		run.ended.insert(run.ended.end(), turn.ended.begin(), turn.ended.end());
		++run.turns;
	}
	return run;
}
} // namespace

TEST(CreatureSpells, TheMagicTypesOfTheCreatureSpellsAreTheirSpells)
{
	EXPECT_EQ(SpellOf(MagicType::CreatureSpellFreeze), Spell::Freeze);
	EXPECT_EQ(SpellOf(MagicType::CreatureSpellBig), Spell::Big);
	EXPECT_EQ(SpellOf(MagicType::CreatureSpellItchy), Spell::Itchy);
	EXPECT_FALSE(SpellOf(MagicType::Fireball).has_value());
	EXPECT_EQ(SpellOf(CreatureReceiveSpellType::CreatureReceiveSpellCompassionate), Spell::Nice);
}

TEST(CreatureSpells, KindsKeepOpposingSpellsApart)
{
	EXPECT_EQ(KindOf(Spell::Small), KindOf(Spell::Big));
	EXPECT_EQ(KindOf(Spell::Weak), KindOf(Spell::Strong));
	EXPECT_EQ(KindOf(Spell::Nice), KindOf(Spell::Nasty));
	EXPECT_EQ(KindOf(Spell::Nice), KindOf(Spell::Itchy));
	EXPECT_EQ(KindOf(Spell::Freeze), KindOf(Spell::Invisible));
	EXPECT_NE(KindOf(Spell::Big), KindOf(Spell::Strong));
}

TEST(CreatureSpells, TheMoodSpellsMakeTheirDesireDominant)
{
	using creature_desires::Desire;
	EXPECT_EQ(EffectOf(Spell::Nice).desire, static_cast<uint8_t>(Desire::Compassion));
	EXPECT_EQ(EffectOf(Spell::Nasty).desire, static_cast<uint8_t>(Desire::Anger));
	EXPECT_EQ(EffectOf(Spell::Itchy).desire, static_cast<uint8_t>(Desire::Scratch));
	EXPECT_FALSE(EffectOf(Spell::Big).desire.has_value());
	EXPECT_EQ(EffectOf(Spell::Freeze).soundAction, 0x76);
	EXPECT_EQ(EffectOf(Spell::Itchy).soundAction, 0x7E);
}

TEST(CreatureSpells, ASpellEasesInHoldsForItsTimeAndEasesOut)
{
	Spells spells;
	const auto result = Receive(spells, Spell::Big, TurnsOf(25.0f, k_TurnsPerSecond), k_Miracle, k_TurnsPerSecond);
	EXPECT_EQ(result.received, Received::Started);
	const auto run = StepUntilOff(spells, Spell::Big);
	// Two seconds' wait, a turn to start, four seconds in, a turn to hold, 25 held, a turn to begin wearing off, four out
	// and a turn to end
	EXPECT_EQ(run.turns, k_DelayTurns + 1 + 40 + 1 + 250 + 1 + 40 + 1);
	ASSERT_GT(run.events.size(), static_cast<size_t>(k_DelayTurns));
	// it holds while it waits, then starts
	for (int turn = 0; turn < k_DelayTurns; ++turn)
	{
		EXPECT_EQ(run.events.at(turn).event, Event::Hold);
	}
	EXPECT_EQ(run.events.at(k_DelayTurns).event, Event::Start);
	EXPECT_EQ(run.events.back().event, Event::Finish);
	EXPECT_EQ(run.ended, std::vector<entt::entity> {k_Miracle});
	// The ratio rises to 1, holds, and falls back to 0
	float last = -1.0f;
	bool rising = true;
	for (const auto& event : run.events)
	{
		if (event.event != Event::Ease)
		{
			continue;
		}
		if (rising && event.ratio < last)
		{
			rising = false;
			EXPECT_NEAR(last, 1.0f, k_Epsilon);
		}
		EXPECT_TRUE(rising ? event.ratio >= last : event.ratio <= last + k_Epsilon);
		last = event.ratio;
	}
	EXPECT_FALSE(rising);
	EXPECT_NEAR(last, 0.0f, k_Epsilon);
}

TEST(CreatureSpells, CastingTheSameSpellAgainAddsItsTimeAndTakesTheNewMiracle)
{
	Spells spells;
	const auto timings = Timings();
	Receive(spells, Spell::Freeze, 100, k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < k_DelayTurns + 30; ++turn)
	{
		(void)Step(spells, timings, k_TurnsPerSecond);
	}
	ASSERT_EQ(spells[Spell::Freeze].phase, Phase::Holding);
	const int left = spells[Spell::Freeze].turnsLeft;
	const auto result = Receive(spells, Spell::Freeze, 50, k_Other, k_TurnsPerSecond);
	EXPECT_EQ(result.received, Received::Extended);
	EXPECT_EQ(result.replaced, k_Miracle);
	EXPECT_EQ(spells[Spell::Freeze].turnsLeft, left + 50);
	EXPECT_EQ(spells[Spell::Freeze].miracle, k_Other);
}

TEST(CreatureSpells, TheOpposingSpellCutsTheFirstShortAndWaitsForIt)
{
	Spells spells;
	const auto timings = Timings();
	Receive(spells, Spell::Small, 250, k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < 100; ++turn)
	{
		(void)Step(spells, timings, k_TurnsPerSecond);
	}
	ASSERT_EQ(spells[Spell::Small].phase, Phase::Holding);
	const auto result = Receive(spells, Spell::Big, 250, k_Other, k_TurnsPerSecond);
	EXPECT_EQ(result.received, Received::Queued);
	EXPECT_FALSE(spells.IsActive(Spell::Big));
	// Small eases out at once, then big waits its two seconds to start. Big is let in as small wears off, and comes
	// after it in the table, so it is counted down that same turn
	auto run = StepUntilOff(spells, Spell::Small);
	EXPECT_EQ(run.turns, 1 + 40 + 1);
	EXPECT_TRUE(spells.IsActive(Spell::Big));
	EXPECT_TRUE(spells.waiting.empty());
	EXPECT_EQ(spells[Spell::Big].miracle, k_Other);
	EXPECT_EQ(spells[Spell::Big].phase, Phase::Waiting);
	EXPECT_EQ(spells[Spell::Big].turnsLeft, k_DelayTurns - 1);
}

TEST(CreatureSpells, ASpellLetInBeforeTheOneItWaitedForIsCountedDownFromTheNextTurn)
{
	Spells spells;
	const auto timings = Timings();
	Receive(spells, Spell::Big, 250, k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < 100; ++turn)
	{
		(void)Step(spells, timings, k_TurnsPerSecond);
	}
	ASSERT_EQ(spells[Spell::Big].phase, Phase::Holding);
	EXPECT_EQ(Receive(spells, Spell::Small, 250, k_Other, k_TurnsPerSecond).received, Received::Queued);
	// Small comes before big in the table, so the turn big wears off has gone past it
	const auto run = StepUntilOff(spells, Spell::Big);
	EXPECT_EQ(run.turns, 1 + 40 + 1);
	EXPECT_EQ(spells[Spell::Small].phase, Phase::Waiting);
	EXPECT_EQ(spells[Spell::Small].turnsLeft, k_DelayTurns);
	EXPECT_EQ(spells[Spell::Small].miracle, k_Other);
}

TEST(CreatureSpells, SpellsOfDifferentKindsRunTogether)
{
	Spells spells;
	EXPECT_EQ(Receive(spells, Spell::Big, 250, k_Miracle, k_TurnsPerSecond).received, Received::Started);
	EXPECT_EQ(Receive(spells, Spell::Strong, 250, k_Other, k_TurnsPerSecond).received, Received::Started);
	EXPECT_TRUE(spells.IsKindActive(Kind::Size));
	EXPECT_TRUE(spells.IsKindActive(Kind::Strength));
	EXPECT_FALSE(spells.IsKindActive(Kind::Mood));
}

TEST(CreatureSpells, ASpellWithoutATimeHoldsUntilCutShort)
{
	Spells spells;
	const auto timings = Timings();
	Receive(spells, Spell::Weak, TurnsOf(0.0f, k_TurnsPerSecond), k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < 1000; ++turn)
	{
		(void)Step(spells, timings, k_TurnsPerSecond);
	}
	EXPECT_EQ(spells[Spell::Weak].phase, Phase::Holding);
	// Strong cuts it short
	Receive(spells, Spell::Strong, 10, k_Other, k_TurnsPerSecond);
	const auto run = StepUntilOff(spells, Spell::Weak);
	EXPECT_LT(run.turns, 100);
}

TEST(CreatureSpells, WithoutReversionASpellStopsWhereItIs)
{
	Spells spells;
	spells.reversion = false;
	Receive(spells, Spell::Big, 5, k_Miracle, k_TurnsPerSecond);
	const auto run = StepUntilOff(spells, Spell::Big);
	EXPECT_FALSE(spells.IsActive(Spell::Big));
	// Waits, starts, eases in over 40 turns, begins to hold, holds 5, then stops at once
	EXPECT_EQ(run.turns, k_DelayTurns + 1 + 40 + 1 + 5 + 1);
	const auto count = [&run](Event event) {
		return std::ranges::count_if(run.events, [event](const TurnEvent& seen) { return seen.event == event; });
	};
	EXPECT_EQ(count(Event::Start), 1);
	EXPECT_EQ(count(Event::BeginFinish), 0);
	EXPECT_EQ(count(Event::Finish), 0);
	EXPECT_EQ(run.ended, (std::vector<entt::entity> {k_Miracle}));
	EXPECT_EQ(spells[Spell::Big].miracle, entt::entity {entt::null});
}

TEST(CreatureSpells, WithoutReversionASpellThatStopsLeavesTheOneWaitingForItWaiting)
{
	Spells spells;
	spells.reversion = false;
	const auto timings = Timings();
	Receive(spells, Spell::Small, 250, k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < 100; ++turn)
	{
		(void)Step(spells, timings, k_TurnsPerSecond);
	}
	ASSERT_EQ(spells[Spell::Small].phase, Phase::Holding);
	EXPECT_EQ(Receive(spells, Spell::Big, 250, k_Other, k_TurnsPerSecond).received, Received::Queued);
	const auto run = StepUntilOff(spells, Spell::Small);
	EXPECT_EQ(run.turns, 1);
	EXPECT_EQ(run.ended, (std::vector<entt::entity> {k_Miracle}));
	// Only a spell that wears off makes way for those waiting
	EXPECT_FALSE(spells.IsActive(Spell::Big));
	ASSERT_EQ(spells.waiting.size(), 1u);
	EXPECT_EQ(spells.waiting.front().miracle, k_Other);
}

TEST(CreatureSpells, TheBodySpellsTargets)
{
	// Big takes a creature to its largest, small to its smallest: 2.4 and 0.2 unless a script sets them
	const SizeLimits limits;
	EXPECT_FLOAT_EQ(limits.smallest, 0.2f);
	EXPECT_FLOAT_EQ(limits.largest, 2.4f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Big, 1.0f, limits), 2.4f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Small, 1.0f, limits), 0.2f);
	// The same from any size within them
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Big, 0.3f, limits), 2.4f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Small, 2.0f, limits), 0.2f);
	// Past the limit it keeps its size: big never shrinks it, small never makes it bigger
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Big, 3.0f, limits), 3.0f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Small, 0.1f, limits), 0.1f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Big, 2.4f, limits), 2.4f);
	// A script's limits
	const SizeLimits set {.smallest = 0.5f, .largest = 1.5f};
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Big, 1.0f, set), 1.5f);
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Small, 1.0f, set), 0.5f);
	// Another spell leaves the size as it was
	EXPECT_FLOAT_EQ(SizeTarget(Spell::Strong, 1.0f, limits), 1.0f);
	EXPECT_EQ(Target(Spell::Weak), k_WeakStrength);
	EXPECT_EQ(Target(Spell::Strong), k_StrongStrength);
	EXPECT_EQ(Target(Spell::Fat), k_FatFatness);
	EXPECT_EQ(Target(Spell::Invisible), k_InvisibleFizz);
	EXPECT_EQ(Target(Spell::Nasty), k_NastyAlignment);
	EXPECT_FALSE(Target(Spell::Itchy).has_value());
	EXPECT_NEAR(Ease(0.5f, 1.0f, 0.5f), 0.75f, k_Epsilon);
}

TEST(CreatureSpells, TheFrozenLookTintsTowardsIcyBlue)
{
	EXPECT_EQ(FrozenTint(0.0f), 0xFFFFFFu);
	EXPECT_EQ(FrozenTint(1.0f), k_FrozenColour);
	const auto half = FrozenTint(0.5f);
	EXPECT_EQ((half >> 16) & 0xFFu, (255u + 0x8Cu + 1u) / 2u);
	EXPECT_EQ(half & 0xFFu, 0xFFu);
}

TEST(CreatureSpells, TimesAreWholeTurns)
{
	EXPECT_EQ(TurnsOf(25.0f, k_TurnsPerSecond), 250);
	EXPECT_EQ(TurnsOf(0.0f, k_TurnsPerSecond), -1);
	// The start delay is two seconds of whole turns
	EXPECT_EQ(StartDelayTurns(k_TurnsPerSecond), k_DelayTurns);
	// Only the whole turns a second count: at 60 ms a turn, 16 a second, so 32 turns of delay and 400 for 25 s
	EXPECT_FLOAT_EQ(WholeTurnsPerSecond(1000.0f / 60.0f), 16.0f);
	EXPECT_EQ(StartDelayTurns(1000.0f / 60.0f), 32);
	EXPECT_EQ(TurnsOf(25.0f, 1000.0f / 60.0f), 400);
	EXPECT_EQ(StartDelayTurns(1000.0f / 30.0f), 66);
	Slot slot {.phase = Phase::Starting, .turnsLeft = 20, .holdTurns = 0, .miracle = entt::null, .before = 0.0f};
	EXPECT_NEAR(Ratio(slot, {.startSeconds = 4.0f, .finishSeconds = 4.0f}, k_TurnsPerSecond), 0.5f, k_Epsilon);
	slot.phase = Phase::Finishing;
	EXPECT_NEAR(Ratio(slot, {.startSeconds = 4.0f, .finishSeconds = 4.0f}, k_TurnsPerSecond), 0.5f, k_Epsilon);
}

TEST(CreatureSpells, ItIsSavedAsItWasBeforeItsSpells)
{
	Spells spells;
	const SavedBody now {.size = 0.2f, .strength = 1.0f, .alignment = -1.0f};
	// Nothing on it: as it is
	auto saved = ValuesToSave(spells, now);
	EXPECT_FLOAT_EQ(saved.size, 0.2f);
	EXPECT_FLOAT_EQ(saved.strength, 1.0f);
	EXPECT_FLOAT_EQ(saved.alignment, -1.0f);
	spells[Spell::Small].phase = Phase::Holding;
	spells[Spell::Small].before = 1.3f;
	spells[Spell::Strong].phase = Phase::Starting;
	spells[Spell::Strong].before = 0.4f;
	spells[Spell::Nasty].phase = Phase::Finishing;
	spells[Spell::Nasty].before = 0.6f;
	saved = ValuesToSave(spells, now);
	EXPECT_FLOAT_EQ(saved.size, 1.3f);
	EXPECT_FLOAT_EQ(saved.strength, 0.4f);
	EXPECT_FLOAT_EQ(saved.alignment, 0.6f);
	// Big before small
	spells[Spell::Big].phase = Phase::Holding;
	spells[Spell::Big].before = 0.9f;
	EXPECT_FLOAT_EQ(ValuesToSave(spells, now).size, 0.9f);
}

TEST(CreatureSpells, ItsSizeBeforeTheSpellsIsBeforeBigElseBeforeSmall)
{
	Spells spells;
	EXPECT_FLOAT_EQ(SizeBeforeSpells(spells, 1.3f), 1.3f);
	spells[Spell::Small].phase = Phase::Holding;
	spells[Spell::Small].before = 0.9f;
	EXPECT_FLOAT_EQ(SizeBeforeSpells(spells, 0.5f), 0.9f);
	spells[Spell::Big].phase = Phase::Starting;
	spells[Spell::Big].before = 1.1f;
	EXPECT_FLOAT_EQ(SizeBeforeSpells(spells, 1.5f), 1.1f);
	// another kind's spell leaves the size as it is
	Spells strong;
	strong[Spell::Strong].phase = Phase::Holding;
	strong[Spell::Strong].before = 0.2f;
	EXPECT_FLOAT_EQ(SizeBeforeSpells(strong, 1.3f), 1.3f);
}

TEST(CreatureSpells, ASizeGivenWhileASizeSpellIsOnIsTheOneItPutsBack)
{
	Spells spells;
	float size = 1.0f;
	SetSizeBeforeSpells(spells, size, 1.2f);
	EXPECT_FLOAT_EQ(size, 1.2f);
	spells[Spell::Small].phase = Phase::Holding;
	spells[Spell::Small].before = 1.2f;
	size = 0.7f;
	SetSizeBeforeSpells(spells, size, 1.4f);
	EXPECT_FLOAT_EQ(size, 0.7f);
	EXPECT_FLOAT_EQ(spells[Spell::Small].before, 1.4f);
	spells[Spell::Big].phase = Phase::Finishing;
	spells[Spell::Big].before = 1.0f;
	SetSizeBeforeSpells(spells, size, 1.6f);
	EXPECT_FLOAT_EQ(size, 0.7f);
	EXPECT_FLOAT_EQ(spells[Spell::Big].before, 1.6f);
	EXPECT_FLOAT_EQ(spells[Spell::Small].before, 1.4f);
}

TEST(CreatureSpells, TheNeutralFieldsLeaveASpellAsItWas)
{
	Spells spells;
	// Reversion is on unless a script turns it off: a spell waits its two seconds before it starts, and still eases out
	EXPECT_TRUE(spells.reversion);
	Receive(spells, Spell::Big, 250, k_Miracle, k_TurnsPerSecond);
	EXPECT_EQ(spells[Spell::Big].phase, Phase::Waiting);
	EXPECT_EQ(spells[Spell::Big].turnsLeft, k_DelayTurns);
	EXPECT_EQ(StepUntilOff(spells, Spell::Big).turns, k_DelayTurns + 1 + 40 + 1 + 250 + 1 + 40 + 1);
}

TEST(CreatureSpells, FaintingBringsEverySpellToItsEnd)
{
	Spells spells;
	const auto timings = Timings();
	// one holding, one still to start
	Receive(spells, Spell::Big, TurnsOf(25.0f, k_TurnsPerSecond), k_Miracle, k_TurnsPerSecond);
	for (int turn = 0; turn < k_DelayTurns + 50; ++turn)
	{
		static_cast<void>(Step(spells, timings, k_TurnsPerSecond));
	}
	ASSERT_EQ(spells[Spell::Big].phase, Phase::Holding);
	Receive(spells, Spell::Strong, 100, k_Other, k_TurnsPerSecond);
	ASSERT_EQ(spells[Spell::Strong].phase, Phase::Waiting);
	EXPECT_FALSE(TryFinishAll(spells));
	EXPECT_EQ(spells[Spell::Big].turnsLeft, 0);
	EXPECT_EQ(spells[Spell::Big].holdTurns, 0);
	EXPECT_EQ(spells[Spell::Strong].holdTurns, 0);
	// the one holding begins to wear off at once
	const auto next = Step(spells, timings, k_TurnsPerSecond);
	EXPECT_EQ(spells[Spell::Big].phase, Phase::Finishing);
	EXPECT_TRUE(std::ranges::any_of(
	    next.events, [](const TurnEvent& event) { return event.spell == Spell::Big && event.event == Event::BeginFinish; }));
	// with none on it, nothing is left
	Spells none;
	EXPECT_TRUE(TryFinishAll(none));
}
