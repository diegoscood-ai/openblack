/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <deque>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureFightHud.h"

using namespace openblack;
using namespace openblack::creature_fight;

namespace
{
/// Random numbers given in turn, then 0
Random Sequence(std::deque<uint32_t> values)
{
	return [values = std::move(values)](uint32_t range) mutable {
		if (values.empty())
		{
			return 0u;
		}
		const auto value = values.front();
		values.pop_front();
		return range == 0 ? 0u : value % range;
	};
}

Random Always(uint32_t value)
{
	return [value](uint32_t range) { return range == 0 ? 0u : std::min(value, range - 1); };
}
} // namespace

TEST(CreatureFight, ArenaIsSizedByTheBiggerCreatureAndCapped)
{
	EXPECT_FLOAT_EQ(ArenaRadius(1.0f, 0.5f), 39.0f);
	EXPECT_FLOAT_EQ(ArenaRadius(0.5f, 1.2f), 39.0f * 1.2f);
	EXPECT_FLOAT_EQ(ArenaRadius(3.0f, 1.0f), 60.0f);
	// the units first, then the factor: at 0.3 a bit below 0.3 x 39
	EXPECT_EQ(std::bit_cast<uint32_t>(ArenaRadius(0.3f, 0.2f)), 0x413B3333u);
	EXPECT_EQ(std::bit_cast<uint32_t>(ArenaRadius(0.2f, 0.3f)), 0x413B3333u);
	EXPECT_EQ(ArenaRadius(-1.0f, -2.0f), 0.0f);
	// a size not a number is taken when it is the first
	EXPECT_EQ(ArenaRadius(std::nanf(""), 1.0f), 0.0f);
	EXPECT_EQ(ArenaRadius(1.0f, std::nanf("")), 39.0f);
	const auto arena = MakeArena({0.0f, 0.0f}, {40.0f, 20.0f}, 1.0f, 1.0f);
	EXPECT_EQ(arena.centre, glm::vec2(20.0f, 10.0f));
	EXPECT_EQ(ArenaSpot(arena, 1.0f, true), glm::vec2(35.0f, 10.0f));
	EXPECT_EQ(ArenaSpot(arena, 2.0f, false), glm::vec2(-10.0f, 10.0f));
	EXPECT_FLOAT_EQ(ArrivalDistance(1.0f), 22.5f);
	// the arena's sound plays at its middle as a map position keeps it, a little short of a middle off the grid
	const auto offGrid = MakeArena({0.0f, 0.0f}, {40.1f, 20.1f}, 1.0f, 1.0f);
	const auto soundAt = ArenaSoundPoint(offGrid);
	EXPECT_EQ(soundAt, glm::vec2(map_coords::Quantise(offGrid.centre.x), map_coords::Quantise(offGrid.centre.y)));
	EXPECT_LT(soundAt.x, offGrid.centre.x);
	EXPECT_EQ(k_ArenaTakenSample, 176);
}

TEST(CreatureFight, InTheStanceTwoTooCloseArePulledApartByHalfTheOverlapAlongTheFightersFacing)
{
	// Reaches 3 and 2 ahead: nearer than 5, each moves half the overlap along the fighter's facing
	const auto push = PullApartOf({0.0f, 0.0f}, {4.0f, 0.0f}, {1.0f, 0.0f}, 3.0f, 2.0f);
	ASSERT_TRUE(push.has_value());
	EXPECT_NEAR(push->x, 0.5f, 1e-6f);
	EXPECT_NEAR(push->y, 0.0f, 1e-6f);
	// Along the facing even when the opponent stands off it; far enough apart, nothing
	const auto aside = PullApartOf({0.0f, 0.0f}, {0.0f, 1.0f}, {1.0f, 0.0f}, 3.0f, 2.0f);
	ASSERT_TRUE(aside.has_value());
	EXPECT_NEAR(aside->x, 2.0f, 1e-6f);
	EXPECT_FALSE(PullApartOf({0.0f, 0.0f}, {5.0f, 0.0f}, {1.0f, 0.0f}, 3.0f, 2.0f).has_value());
}

TEST(CreatureFight, StepsBackOrSidewaysOnlyWithinRange)
{
	const Arena arena {.centre = {0.0f, 0.0f}, .radius = 50.0f};
	EXPECT_TRUE(WithinRange(arena, {39.0f, 0.0f}));
	EXPECT_FALSE(WithinRange(arena, {41.0f, 0.0f}));
	EXPECT_TRUE(CanStep(arena, {45.0f, 0.0f}, Step::Forward));
	EXPECT_FALSE(CanStep(arena, {45.0f, 0.0f}, Step::Back));
	EXPECT_FALSE(CanStep(arena, {45.0f, 0.0f}, Step::Left));
	EXPECT_TRUE(CanStep(arena, {10.0f, 0.0f}, Step::Right));
	// a block's recoil moves it only strictly inside the range: on its edge it stays
	EXPECT_TRUE(StrictlyWithinRange(arena, {39.0f, 0.0f}));
	EXPECT_FALSE(StrictlyWithinRange(arena, {40.0f, 0.0f}));
	EXPECT_TRUE(WithinRange(arena, {40.0f, 0.0f}));
	// any other move carries it wherever it goes, past the range too, as a blow's reel does
	EXPECT_EQ(ActionMoveTo(arena, State::Action, {35.0f, 0.0f}, {10.0f, 0.0f}), glm::vec2(45.0f, 0.0f));
	EXPECT_EQ(ActionMoveTo(arena, State::BlockRecoil, {35.0f, 0.0f}, {4.0f, 0.0f}), glm::vec2(39.0f, 0.0f));
	EXPECT_EQ(ActionMoveTo(arena, State::BlockRecoil, {35.0f, 0.0f}, {10.0f, 0.0f}), glm::vec2(35.0f, 0.0f));
}

TEST(CreatureFight, ClickHeightPicksTheBand)
{
	EXPECT_EQ(BandOf(0.1f), Band::Low);
	EXPECT_EQ(BandOf(0.32f), Band::Low);
	EXPECT_EQ(BandOf(0.33f), Band::Mid);
	EXPECT_EQ(BandOf(0.5f), Band::Mid);
	EXPECT_EQ(BandOf(0.66f), Band::Mid);
	EXPECT_EQ(BandOf(0.67f), Band::High);
	EXPECT_EQ(AttackMove(Band::High).kind, Move::Kind::High);
	EXPECT_EQ(AttackMove(Band::Low).kind, Move::Kind::Low);
}

// The step a press on the ground makes, against the original's own answers (the oracle, 2026-10-29): the creature's
// place, its heading as openblack keeps it (the other way round from the game's), the point, and the step. A point as
// far ahead as to the side, or on the creature itself, is a step left
TEST(CreatureFight, AGroundPressStepsAsTheOriginal)
{
	struct Case
	{
		uint32_t x, z, heading, pointX, pointZ;
		Step step;
	};
	constexpr std::array k_Cases {
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42c80000, 0x42b40000, Step::Forward},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42c80000, 0x42dc0000, Step::Back},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42dc0000, 0x42c80000, Step::Left},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42b40000, 0x42c80000, Step::Right},
	    Case {0x42c80000, 0x42c80000, 0xbfc90fdb, 0x42dc0000, 0x42c80000, Step::Forward},
	    Case {0x42c80000, 0x42c80000, 0xbfc90fdb, 0x42c80000, 0x42b40000, Step::Right},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42dc0000, 0x42b40000, Step::Left},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42b40000, 0x42b40000, Step::Left},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42dc0000, 0x42dc0000, Step::Left},
	    Case {0x42c80000, 0x42c80000, 0x00000000, 0x42c80000, 0x42c80000, Step::Left},
	    Case {0x451c2f71, 0x452dcdf0, 0xc01fe0bb, 0x451a859a, 0x452e4b50, Step::Left},
	    Case {0x451c2f71, 0x452dcdf0, 0x3f24bc7d, 0x451ce000, 0x452d2000, Step::Left},
	    Case {0x43fa2000, 0x44163000, 0xbf490fd8, 0x43fc8000, 0x44174000, Step::Left},
	    Case {0x43fa2000, 0x44163000, 0x4016cbe2, 0x43f7c000, 0x44138000, Step::Left},
	    Case {0x43fa2000, 0x44163000, 0xbf490fd8, 0x43f50000, 0x44190000, Step::Back},
	    Case {0x43fa2000, 0x44163000, 0xbf490fd8, 0x43ff0000, 0x44138000, Step::Forward},
	    Case {0x43fa2000, 0x44163000, 0xbf490fd8, 0x43f70000, 0x44148000, Step::Right},
	    Case {0x451c2f71, 0x452dcdf0, 0xc01fe0bb, 0x451ce000, 0x452d3000, Step::Right},
	    Case {0x451c2f71, 0x452dcdf0, 0xc01fe0bb, 0x451ba000, 0x452d2000, Step::Back},
	    Case {0x451c2f71, 0x452dcdf0, 0xc01fe0bb, 0x451c4000, 0x452f0000, Step::Forward},
	    Case {0x42c80000, 0x42c80000, 0x3f800000, 0x42ca0000, 0x42c60000, Step::Left},
	    Case {0x42c80000, 0x42c80000, 0xc0400000, 0x42c90000, 0x42cc0000, Step::Forward},
	};
	for (const auto& c : k_Cases)
	{
		const glm::vec2 at {std::bit_cast<float>(c.x), std::bit_cast<float>(c.z)};
		const glm::vec2 point {std::bit_cast<float>(c.pointX), std::bit_cast<float>(c.pointZ)};
		EXPECT_EQ(GroundStep(point, at, std::bit_cast<float>(c.heading)), c.step) << c.pointX << " " << c.pointZ;
	}
	EXPECT_EQ(StepMove(Step::Left).value, animations::k_StepLeft);
	EXPECT_EQ(StepOf(animations::k_StepBack), Step::Back);
	EXPECT_FALSE(StepOf(animations::k_AttackHigh).has_value());
}

TEST(CreatureFight, ChargeRunsToOnePointTwoSeconds)
{
	EXPECT_FALSE(InitialCharge(Move::Kind::Mid).has_value());
	EXPECT_EQ(InitialCharge(Move::Kind::Block), 0.0f);
	EXPECT_EQ(InitialCharge(Move::Kind::Animation), 0.0f);
	EXPECT_EQ(InitialCharge(Move::Kind::Special), k_MaxChargeMs);
	EXPECT_EQ(InitialCharge(Move::Kind::Spell), k_MaxChargeMs);
	EXPECT_FLOAT_EQ(ReleasedCharge(5000.0f), 1200.0f);
	EXPECT_FLOAT_EQ(ReleasedCharge(-3.0f), 0.0f);
	EXPECT_FLOAT_EQ(BlowSpeed(0.0f), 0.5f);
	EXPECT_FLOAT_EQ(BlowSpeed(600.0f), 1.0f);
	EXPECT_FLOAT_EQ(BlowSpeed(3000.0f), 1.5f);
}

TEST(CreatureFight, QueueHoldsTwelveAndAClickReplacesIt)
{
	MoveQueue queue;
	for (size_t i = 0; i < MoveQueue::k_Capacity; ++i)
	{
		EXPECT_TRUE(queue.Push(BlockMove(), false));
	}
	EXPECT_FALSE(queue.Push(BlockMove(), false));
	EXPECT_EQ(queue.Size(), 12u);
	EXPECT_TRUE(queue.Push(AttackMove(Band::High), true));
	EXPECT_EQ(queue.Size(), 1u);
	EXPECT_TRUE(queue.AwaitsRelease());
	EXPECT_TRUE(queue.Release(700.0f));
	EXPECT_FALSE(queue.AwaitsRelease());
	EXPECT_EQ(queue.Front()->chargeMs, 700.0f);
	EXPECT_FALSE(queue.Release(100.0f));
	queue.Push(StepMove(Step::Back), false);
	queue.Pop();
	EXPECT_EQ(queue.Front()->move, StepMove(Step::Back));
	queue.Pop();
	EXPECT_TRUE(queue.Empty());
	queue.Pop();
	EXPECT_TRUE(queue.Empty());
}

// Only the move added last can be let go, while it is a blow waiting: a blow followed by any other move waits for good,
// and a move pushed on a full queue lets nothing be let go any more
TEST(CreatureFight, OnlyTheBlowAddedLastIsLetGo)
{
	MoveQueue queue;
	queue.Push(AttackMove(Band::High), false);
	queue.Push(StepMove(Step::Back), false);
	EXPECT_FALSE(queue.AwaitsRelease());
	EXPECT_FALSE(queue.Release(700.0f));
	EXPECT_FALSE(queue.Front()->chargeMs.has_value());
	queue.Push(AttackMove(Band::Low), false);
	EXPECT_TRUE(queue.Release(300.0f));
	EXPECT_EQ(queue.Moves()[2].chargeMs, 300.0f);
	EXPECT_FALSE(queue.Moves()[0].chargeMs.has_value());

	MoveQueue full;
	for (size_t i = 0; i + 1 < MoveQueue::k_Capacity; ++i)
	{
		full.Push(BlockMove(), false);
	}
	EXPECT_TRUE(full.Push(AttackMove(Band::Mid), false));
	EXPECT_TRUE(full.AwaitsRelease());
	EXPECT_FALSE(full.Push(BlockMove(), false));
	EXPECT_FALSE(full.AwaitsRelease());
	EXPECT_FALSE(full.Release(500.0f));
	EXPECT_FALSE(full.Moves().back().chargeMs.has_value());
}

// A hit takes away only the move added last, and only while it is a blow waiting to be let go
TEST(CreatureFight, GettingHitTakesAWaitingBlowAway)
{
	MoveQueue queue;
	queue.Push(BlockMove(), false);
	queue.Push(AttackMove(Band::Mid), false);
	queue.Push(StepMove(Step::Forward), false);
	EXPECT_FALSE(queue.TakeWaitingBlow());
	ASSERT_EQ(queue.Size(), 3u);
	queue.Push(AttackMove(Band::High), false);
	EXPECT_TRUE(queue.TakeWaitingBlow());
	ASSERT_EQ(queue.Size(), 3u);
	EXPECT_EQ(queue.Moves()[0].move, BlockMove());
	EXPECT_EQ(queue.Moves()[1].move, AttackMove(Band::Mid));
	EXPECT_EQ(queue.Moves()[2].move, StepMove(Step::Forward));
	EXPECT_FALSE(queue.AwaitsRelease());
	EXPECT_FALSE(queue.TakeWaitingBlow());
}

TEST(CreatureFight, OrdersAreTakenInRangeAndOnceCharged)
{
	Fighter fighter;
	Enter(fighter, State::Stance);
	fighter.queue.Push(AttackMove(Band::Low), false);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
	fighter.queue.Release(1200.0f);
	EXPECT_FALSE(TakeOrder(fighter, false).has_value());
	const auto order = TakeOrder(fighter, true);
	ASSERT_TRUE(order.has_value());
	EXPECT_EQ(order->kind, Order::Kind::Blow);
	EXPECT_EQ(order->band, Band::Low);
	EXPECT_FLOAT_EQ(order->speed, 1.5f);
	// A blow stays queued until it is thrown, and is offered again from the next stance
	EXPECT_EQ(fighter.queue.Size(), 1u);
	EXPECT_EQ(TakeOrder(fighter, true)->kind, Order::Kind::Blow);
	fighter.queue.Pop();

	// Blocking, anything ends the block and is made after it
	Enter(fighter, State::Block);
	fighter.queue.Push(StepMove(Step::Back), false);
	EXPECT_EQ(TakeOrder(fighter, true)->kind, Order::Kind::EndBlock);
	EXPECT_EQ(fighter.queue.Size(), 1u);
	Enter(fighter, State::Action, animations::k_AttackHigh);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
}

TEST(CreatureFight, TheComputerTakesOverOnlyInTheStanceOrTheBlock)
{
	Fighter fighter;
	fighter.control = Control::Player;
	fighter.computerWaitMs = 150.0f;
	// Its moves and its reels in the block count the wait down, past its end, but the computer waits for the stance
	Enter(fighter, State::Action, animations::k_AttackHigh);
	EXPECT_FALSE(CountComputerWait(fighter, 100.0f));
	Enter(fighter, State::BlockRecoil);
	EXPECT_FALSE(CountComputerWait(fighter, 100.0f));
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, -50.0f);
	EXPECT_EQ(fighter.control, Control::Player);
	// The start and the end of a block, a cast and the faint do not count it at all
	for (const auto state : {State::BlockStart, State::BlockEnd, State::CastStart, State::Cast, State::Faint})
	{
		Enter(fighter, state);
		EXPECT_FALSE(CountComputerWait(fighter, 100.0f));
		EXPECT_FLOAT_EQ(fighter.computerWaitMs, -50.0f);
	}
	Enter(fighter, State::Stance);
	EXPECT_TRUE(CountComputerWait(fighter, 100.0f));
	EXPECT_EQ(fighter.control, Control::Computer);
	// In the block too, once the wait is below 0, and not at 0
	fighter.control = Control::Player;
	fighter.computerWaitMs = 100.0f;
	Enter(fighter, State::Block);
	EXPECT_FALSE(CountComputerWait(fighter, 100.0f));
	EXPECT_TRUE(CountComputerWait(fighter, 100.0f));
	// The computer's own creature and one nobody fights for count nothing
	for (const auto control : {Control::Computer, Control::None})
	{
		fighter.control = control;
		fighter.computerWaitMs = 100.0f;
		EXPECT_FALSE(CountComputerWait(fighter, 200.0f));
		EXPECT_FLOAT_EQ(fighter.computerWaitMs, 100.0f);
		EXPECT_EQ(fighter.control, control);
	}
}

TEST(CreatureFight, AQueuedMoveInRangeMakesTheComputerWaitAgain)
{
	Fighter fighter;
	fighter.control = Control::Player;
	Enter(fighter, State::Stance);
	fighter.queue.Push(AttackMove(Band::High), false);
	// Out of range it does not
	fighter.computerWaitMs = 10.0f;
	EXPECT_FALSE(TakeOrder(fighter, false).has_value());
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, 10.0f);
	// In range, also a blow that stays in the queue until it is let go
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, k_ComputerWaitsMs);
	EXPECT_EQ(fighter.queue.Size(), 1u);
	// In the block, the move that ends it
	fighter.computerWaitMs = 10.0f;
	Enter(fighter, State::Block);
	EXPECT_EQ(TakeOrder(fighter, true)->kind, Order::Kind::EndBlock);
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, k_ComputerWaitsMs);
	// In a move, it does not
	fighter.computerWaitMs = 10.0f;
	Enter(fighter, State::Action, animations::k_AttackHigh);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, 10.0f);
	// Nor with nothing queued
	fighter.queue.Clear();
	Enter(fighter, State::Stance);
	EXPECT_FALSE(TakeOrder(fighter, true).has_value());
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, 10.0f);
}

TEST(CreatureFight, PlayerMovesTakeBackControlAndTeach)
{
	Fighter fighter;
	fighter.control = Control::Computer;
	fighter.computerWaitMs = 0.0f;
	EXPECT_TRUE(PlayerMove(fighter, AttackMove(Band::High), true));
	EXPECT_EQ(fighter.control, Control::Player);
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, k_ComputerWaitsMs);
	// queuing teaches nothing: the creature learns as it looks at the move in its queue
	EXPECT_FLOAT_EQ(fighter.tendency, 0.0f);
	EXPECT_FLOAT_EQ(LearnTendency(0.0f, Move::Kind::High), 0.02f);
	EXPECT_FLOAT_EQ(LearnTendency(0.02f, Move::Kind::Block), (0.98f * 0.02f) - 0.02f);
	EXPECT_FLOAT_EQ(LearnTendency(1.0f, Move::Kind::Mid), 1.0f);
	EXPECT_FLOAT_EQ(LearnTendency(-1.0f, Move::Kind::Block), -1.0f);
	EXPECT_FLOAT_EQ(FirstTendency(-0.7f), 0.7f);
	// against its alignment until it has fought, then what it learnt
	EXPECT_FLOAT_EQ(TendencyAtStart(false, 0.4f, 0.5f), -0.5f);
	EXPECT_FLOAT_EQ(TendencyAtStart(true, 0.4f, 0.5f), 0.4f);
}

TEST(CreatureFight, AStepTeachesDefenceAndAMiracleTeachesNothing)
{
	// The game takes a step from the queue and nudges the lean as it does a block's, towards defence; taking a miracle
	// it leaves the lean alone, and the special move nudges it as a blow does
	EXPECT_FLOAT_EQ(LearnTendency(0.5f, Move::Kind::Animation), (0.98f * 0.5f) - 0.02f);
	EXPECT_FLOAT_EQ(LearnTendency(0.5f, Move::Kind::Spell), 0.5f);
	EXPECT_FLOAT_EQ(LearnTendency(0.5f, Move::Kind::Special), (0.98f * 0.5f) + 0.02f);
	EXPECT_FLOAT_EQ(LearnTendency(-1.0f, Move::Kind::Animation), -1.0f);
}

// A step teaches every creature, a miracle none, and the other moves only the local player's creature
TEST(CreatureFight, OnlyTheLocalPlayersCreatureLearnsFromAllButStepsAndMiracles)
{
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Animation, false));
	EXPECT_TRUE(LearnsFromMove(Move::Kind::Animation, true));
	EXPECT_FALSE(LearnsFromMove(Move::Kind::Spell, true));
	for (const auto kind : {Move::Kind::High, Move::Kind::Mid, Move::Kind::Low, Move::Kind::Block, Move::Kind::Special})
	{
		EXPECT_TRUE(LearnsFromMove(kind, true));
		EXPECT_FALSE(LearnsFromMove(kind, false));
	}
}

TEST(CreatureFight, ScriptsNumberTheirMoves)
{
	EXPECT_EQ(ScriptMove(0), AttackMove(Band::High));
	EXPECT_EQ(ScriptMove(1), AttackMove(Band::Mid));
	EXPECT_EQ(ScriptMove(2), AttackMove(Band::Low));
	EXPECT_EQ(ScriptMove(3), BlockMove());
	EXPECT_EQ(ScriptMove(4), (Move {.kind = Move::Kind::Special}));
	EXPECT_FALSE(ScriptMove(5).has_value());
	// The animation bit plays an animation by its number, the spell bit casts a miracle; the animation bit wins
	EXPECT_EQ(ScriptMove(0x8000 | 139), (Move {.kind = Move::Kind::Animation, .value = 139}));
	EXPECT_EQ(ScriptMove(0x4000 | 7), (Move {.kind = Move::Kind::Spell, .value = 7}));
	EXPECT_EQ(ScriptMove(0xC000 | 140), (Move {.kind = Move::Kind::Animation, .value = 140}));
	EXPECT_EQ(ScriptMove(0x8000 | 0x7FFF), (Move {.kind = Move::Kind::Animation, .value = 0x3FFF}));
}

TEST(CreatureFight, TheFightActionIsTheGamesNumberForItsAnimation)
{
	// The game's table, by animation from 106 to 193; every other animation is 10
	struct Span
	{
		size_t first;
		size_t last;
		FightAction action;
	};
	constexpr std::array k_Spans {
	    Span {106, 106, FightAction::Fainting},      Span {110, 112, FightAction::Casting},
	    Span {122, 124, FightAction::Stance},        Span {125, 129, FightAction::Blow},
	    Span {130, 130, FightAction::Special},       Span {131, 135, FightAction::Blow},
	    Span {136, 136, FightAction::Special},       Span {137, 140, FightAction::Step},
	    Span {142, 144, FightAction::Block},         Span {179, 183, FightAction::ReelingHigh},
	    Span {184, 188, FightAction::ReelingMiddle}, Span {189, 193, FightAction::ReelingLow},
	};
	for (size_t animation = 0; animation < 220; ++animation)
	{
		const auto span = std::ranges::find_if(
		    k_Spans, [animation](const Span& each) { return animation >= each.first && animation <= each.last; });
		const auto expected = span != k_Spans.end() ? span->action : FightAction::Other;
		EXPECT_EQ(FightActionOf(animation), expected) << "animation " << animation;
	}
	EXPECT_EQ(static_cast<int>(FightAction::Block), 1);
	EXPECT_EQ(static_cast<int>(FightAction::ReelingHigh), 6);
	EXPECT_EQ(static_cast<int>(FightAction::ReelingLow), 8);
	EXPECT_EQ(static_cast<int>(FightAction::Other), 10);
}

TEST(CreatureFight, WhatAFighterCountsAsDoing)
{
	Fighter fighter;
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Other);
	// Its start keeps what it counted before; its stance counts nothing
	Enter(fighter, State::Start);
	EXPECT_EQ(fighter.actionAnimation, k_NoActionAnimation);
	Enter(fighter, State::Stance);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Other);
	// An action counts until it is back in its stance
	Enter(fighter, State::Action, animations::k_AttackBelly);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Blow);
	Enter(fighter, State::Stance);
	EXPECT_EQ(fighter.actionAnimation, k_NoActionAnimation);
	Enter(fighter, State::Action, animations::k_RecoilHigh + 2);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::ReelingHigh);
	// A block counts as the block itself from its start to its end, unless it reels in it
	Enter(fighter, State::BlockStart);
	EXPECT_EQ(fighter.actionAnimation, animations::k_Block);
	Enter(fighter, State::Block);
	Enter(fighter, State::BlockEnd);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Block);
	Enter(fighter, State::BlockStart);
	Enter(fighter, State::Block);
	Enter(fighter, State::BlockRecoil);
	Enter(fighter, State::Block);
	EXPECT_EQ(fighter.actionAnimation, animations::k_RecoilBlock);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Other);
	// A cast counts as the cast from its start to its end
	Enter(fighter, State::Stance);
	Enter(fighter, State::CastStart);
	Enter(fighter, State::Cast);
	Enter(fighter, State::CastEnd);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Casting);
	// A finish keeps what it counted; out of the fight it counts nothing
	Enter(fighter, State::Finish);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Casting);
	Enter(fighter, State::Idle);
	EXPECT_EQ(fighter.actionAnimation, k_NoActionAnimation);
	// A faint counts until it gets up
	Enter(fighter, State::Faint);
	Enter(fighter, State::Lying);
	EXPECT_EQ(FightActionOf(fighter.actionAnimation), FightAction::Fainting);
	Enter(fighter, State::GetUp);
	EXPECT_EQ(fighter.actionAnimation, animations::k_GetUp);
}

TEST(CreatureFight, OnlyBlowsAtABandCountAsQueuedHits)
{
	MoveQueue queue;
	EXPECT_EQ(QueuedBlows(queue.Moves()), 0u);
	queue.Push(AttackMove(Band::High), false);
	queue.Push(BlockMove(), false);
	queue.Push(StepMove(Step::Left), false);
	queue.Push({.kind = Move::Kind::Special}, false);
	queue.Push({.kind = Move::Kind::Spell, .value = 3}, false);
	queue.Push(AttackMove(Band::Low), false);
	queue.Push(AttackMove(Band::Mid), false);
	EXPECT_EQ(QueuedBlows(queue.Moves()), 3u);
}

TEST(CreatureFight, NobodyFightsForACreatureTheScriptLeftAlone)
{
	// A move queued for a creature nobody fights for leaves it so; one the computer fought for goes to the player
	Fighter fighter;
	fighter.control = Control::None;
	fighter.computerWaitMs = 0.0f;
	EXPECT_TRUE(PlayerMove(fighter, BlockMove(), false));
	EXPECT_EQ(fighter.control, Control::None);
	EXPECT_FLOAT_EQ(fighter.computerWaitMs, k_ComputerWaitsMs);
	fighter.control = Control::Computer;
	EXPECT_TRUE(ScriptQueueMove(fighter, BlockMove()));
	EXPECT_EQ(fighter.control, Control::Player);
}

TEST(CreatureFight, AScriptsMovesQueueAtTheBackAndTeachNothingAsTheyAreQueued)
{
	Fighter other;
	other.tendency = 0.5f;
	ScriptQueueMove(other, BlockMove());
	ScriptQueueMove(other, AttackMove(Band::Mid));
	ScriptQueueMove(other, {.kind = Move::Kind::Special});
	ScriptQueueMove(other, StepMove(Step::Forward));
	EXPECT_FLOAT_EQ(other.tendency, 0.5f);
	// Queued at the back, never in place of the rest
	EXPECT_EQ(other.queue.Size(), 4u);
}

TEST(CreatureFight, HealthStaminaAndLife)
{
	EXPECT_FLOAT_EQ(FightHealthAtStart(1.0f), 1.0f);
	EXPECT_FLOAT_EQ(FightHealthAtStart(0.2f), 0.6f);
	EXPECT_FLOAT_EQ(StaminaAtStart(0.8f, 0.3f), 0.5f);
	EXPECT_FLOAT_EQ(StaminaAtStart(0.2f, 0.9f), 0.0f);
	EXPECT_FLOAT_EQ(RegainStamina(1.0f), 1.0f);
	EXPECT_EQ(std::bit_cast<uint32_t>(k_StaminaPerTurn), 0x3ADA71D1u);
	EXPECT_EQ(RegainStamina(0.5f), 0.5f + 0.0016666f);
	// A knock out costs a quarter of the bar
	EXPECT_FLOAT_EQ(LifeAfterFight(1.0f, 0.0f), 0.75f);
	EXPECT_FLOAT_EQ(LifeAfterFight(0.8f, 0.6f), 0.7f);
	// nothing keeps the life or the fight health within 0 and 1: below none, a blow's knock-out costs more
	EXPECT_FLOAT_EQ(LifeAfterFight(0.1f, 0.0f), -0.15f);
	EXPECT_FLOAT_EQ(LifeAfterFight(1.0f, -0.2f), 0.7f);
	EXPECT_FLOAT_EQ(HealthAfterBlow(0.03f, 0.05f), -0.02f);
	EXPECT_TRUE(HealthyEnoughToFight(0.11f));
	EXPECT_FALSE(HealthyEnoughToFight(0.1f));
	EXPECT_FLOAT_EQ(FaintSeconds(1.0f), 12.0f);
	EXPECT_FLOAT_EQ(FaintSeconds(2.0f), 16.0f);
	EXPECT_TRUE(NeedsRest(0.3f, 0.0f));
	EXPECT_TRUE(NeedsRest(0.9f, 0.5f));
	EXPECT_FALSE(NeedsRest(0.4f, 0.3f));
}

// A miracle's heal goes on the fight health, kept within 0 and 1, then its harm comes off, unclamped, and harmed to none
// or less the fighter is knocked out; a heal alone never knocks one out
TEST(CreatureFight, AMiraclesHealThenItsHarmGoToTheFightHealth)
{
	const auto healed = TakeMiracle(0.5f, 0.7f, 0.0f);
	EXPECT_FLOAT_EQ(healed.health, 1.0f);
	EXPECT_FALSE(healed.knockedOut);
	const auto both = TakeMiracle(0.5f, 0.7f, 0.25f);
	EXPECT_FLOAT_EQ(both.health, 0.75f);
	EXPECT_FALSE(both.knockedOut);
	const auto out = TakeMiracle(0.3f, 0.0f, 0.5f);
	EXPECT_FLOAT_EQ(out.health, -0.2f);
	EXPECT_TRUE(out.knockedOut);
	EXPECT_TRUE(TakeMiracle(0.3f, 0.0f, 0.3f).knockedOut);
	// below none, a small heal brings it to none, which is no knock-out without harm
	const auto raised = TakeMiracle(-0.2f, 0.1f, 0.0f);
	EXPECT_FLOAT_EQ(raised.health, 0.0f);
	EXPECT_FALSE(raised.knockedOut);
	EXPECT_FLOAT_EQ(k_BlockedMiracleShare, 0.1f);
}

// Life taken from a fighter by anything else comes off its fight health, never below none; life given goes on it, never
// above 1, and a fight health below none is raised by all of it
TEST(CreatureFight, LifeLostOrGainedInADuelMovesTheFightHealth)
{
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(0.5f, 0.2f), 0.3f);
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(0.3f, 0.5f), 0.0f);
	EXPECT_FLOAT_EQ(HealthAfterLifeLost(-0.2f, 0.1f), 0.0f);
	EXPECT_FLOAT_EQ(HealthAfterLifeGained(0.5f, 0.2f), 0.7f);
	EXPECT_FLOAT_EQ(HealthAfterLifeGained(0.9f, 0.5f), 1.0f);
	EXPECT_FLOAT_EQ(HealthAfterLifeGained(-0.5f, 0.2f), -0.3f);
}

TEST(CreatureFight, DamageFollowsTheFormula)
{
	// Even creatures at normal speed: high 0.05, middle and low 0.03
	Blow even {.heightShare = 0.8f};
	EXPECT_FLOAT_EQ(DamageFactor(even), 1.0f);
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.05f);
	even.heightShare = 0.5f;
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.03f);
	even.heightShare = 0.1f;
	EXPECT_FLOAT_EQ(ResolveBlow(even, RecoilDirection::Plain).damage, 0.03f);

	// Size, strength and speed scale it; each shown strength, from -1 to 1, adds 3 before they are compared
	const Blow strong {
	    .attackerSize = 1.0f, .defenderSize = 1.0f, .attackerStrength = 1.0f, .defenderStrength = 0.0f, .speed = 1.0f};
	EXPECT_FLOAT_EQ(DamageFactor(strong), 4.0f / 3.0f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerStrength = -1.0f, .defenderStrength = 1.0f}), 0.5f);
	// The strengths first, then the sizes, then the speed, each step a float as the game's
	const Blow mixed {
	    .attackerSize = 1.3f, .defenderSize = 0.7f, .attackerStrength = -0.168f, .defenderStrength = 0.84f, .speed = 1.1f};
	const float strengths = (mixed.attackerStrength + 3.0f) / (mixed.defenderStrength + 3.0f);
	const float sizes = mixed.attackerSize / mixed.defenderSize;
	EXPECT_EQ(DamageFactor(mixed), (strengths * sizes) * mixed.speed);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 2.0f, .defenderSize = 1.0f, .speed = 0.5f}), 1.0f);
	// Kept from 0.04 to 2, doubled for the special move
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 4.0f, .defenderSize = 1.0f, .speed = 1.5f}), 2.0f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 4.0f, .defenderSize = 1.0f, .speed = 1.5f, .special = true}), 4.0f);
	EXPECT_FLOAT_EQ(DamageFactor({.attackerSize = 0.1f, .defenderSize = 2.0f, .speed = 0.5f}), 0.04f);
	// The most a blow does: 0.1, or 0.2 for the special move
	EXPECT_FLOAT_EQ(ResolveBlow({.attackerSize = 4.0f, .speed = 1.5f, .heightShare = 0.9f}, RecoilDirection::Plain).damage,
	                0.1f);
	EXPECT_FLOAT_EQ(
	    ResolveBlow({.attackerSize = 4.0f, .speed = 1.5f, .special = true, .heightShare = 0.9f}, RecoilDirection::Plain).damage,
	    0.2f);
}

TEST(CreatureFight, ABlockedBlowDoesATenth)
{
	const auto open = ResolveBlow({.heightShare = 0.8f}, RecoilDirection::Plain);
	const auto blocked = ResolveBlow({.heightShare = 0.8f, .blocked = true}, RecoilDirection::Plain);
	EXPECT_FLOAT_EQ(blocked.damage, open.damage * 0.1f);
}

TEST(CreatureFight, RecoilsByHeightAndDirection)
{
	EXPECT_EQ(ResolveBlow({.heightShare = 0.9f}, RecoilDirection::Plain).recoil, animations::k_RecoilHigh);
	EXPECT_EQ(ResolveBlow({.heightShare = 0.5f}, RecoilDirection::Right).recoil, animations::k_RecoilMid + 1);
	const auto low = ResolveBlow({.heightShare = 0.1f}, RecoilDirection::Bottom);
	EXPECT_EQ(low.recoil, animations::k_RecoilLow + 4);
	EXPECT_EQ(low.plainRecoil, animations::k_RecoilLow);
	EXPECT_EQ(RecoilDirectionOf({0.1f, 0.1f}), RecoilDirection::Plain);
	EXPECT_EQ(RecoilDirectionOf({0.8f, 0.2f}), RecoilDirection::Right);
	EXPECT_EQ(RecoilDirectionOf({-0.8f, 0.2f}), RecoilDirection::Left);
	EXPECT_EQ(RecoilDirectionOf({0.1f, 0.9f}), RecoilDirection::Top);
	EXPECT_EQ(RecoilDirectionOf({0.1f, -0.9f}), RecoilDirection::Bottom);
	EXPECT_EQ(WobbleAnimation(Band::High, true), animations::k_WobbleHighSide);
	EXPECT_EQ(WobbleAnimation(Band::Low, false), animations::k_WobbleLowFrontBack);
}

TEST(CreatureFight, WoundsByTheBlow)
{
	EXPECT_EQ(WoundKind(1, Sequence({0})), 0);
	EXPECT_EQ(WoundKind(0, Sequence({1})), 1);
	EXPECT_EQ(WoundKind(2, Sequence({1})), 2);
	EXPECT_EQ(WoundKind(1, Sequence({1, 2})), 5);
	EXPECT_EQ(BlowWoundType(animations::k_AttackEye), 1);
	EXPECT_EQ(BlowWoundType(animations::k_AttackKnee + 6), 2);
	EXPECT_EQ(BlowWoundType(animations::k_AttackBelly), 0);
	EXPECT_TRUE(Bleeds(3));
	EXPECT_FALSE(Bleeds(4));
}

TEST(CreatureFight, ASpellInAFightIsCastAtTheOpponentOrItselfAndNeverWithoutAnOpponent)
{
	EXPECT_EQ(FightSpellTarget(true, false, true), FightSpellAt::Opponent);
	EXPECT_EQ(FightSpellTarget(false, true, true), FightSpellAt::Itself);
	// aggressive comes first
	EXPECT_EQ(FightSpellTarget(true, true, true), FightSpellAt::Opponent);
	EXPECT_EQ(FightSpellTarget(false, false, true), FightSpellAt::Nothing);
	// with no opponent, not even a defensive one
	EXPECT_EQ(FightSpellTarget(true, false, false), FightSpellAt::Nothing);
	EXPECT_EQ(FightSpellTarget(false, true, false), FightSpellAt::Nothing);
}

TEST(CreatureFight, ACastInAFightIsLetGoAfterTheFirstUpdateOfMoreThanTwoMilliseconds)
{
	Fighter fighter;
	EXPECT_TRUE(HoldFightCast(fighter, 100.0f));
	// whole milliseconds are counted: 1.9 counts as 1, so three such updates are needed
	Fighter slow;
	EXPECT_FALSE(HoldFightCast(slow, 1.9f));
	EXPECT_FALSE(HoldFightCast(slow, 1.9f));
	EXPECT_TRUE(HoldFightCast(slow, 1.9f));
	EXPECT_EQ(slow.castMs, 3u);
}

TEST(CreatureFight, StatesFollowOneAnother)
{
	EXPECT_EQ(AfterAnimation(State::Start, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Start, false), State::Finish);
	EXPECT_EQ(AfterAnimation(State::Stance, false), State::Finish);
	EXPECT_EQ(AfterAnimation(State::Action, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Finish, true), State::Idle);
	EXPECT_EQ(AfterAnimation(State::BlockStart, true), State::Block);
	EXPECT_EQ(AfterAnimation(State::BlockRecoil, true), State::Block);
	EXPECT_EQ(AfterAnimation(State::BlockEnd, true), State::Stance);
	EXPECT_EQ(AfterAnimation(State::Faint, true), State::Lying);
	EXPECT_EQ(AfterAnimation(State::CastStart, true), State::Cast);
	EXPECT_EQ(AfterAnimation(State::Cast, true), State::CastEnd);
	EXPECT_EQ(AfterAnimation(State::CastEnd, true), State::Stance);
	EXPECT_EQ(AnimationOf(State::BlockRecoil), animations::k_RecoilBlock);
	EXPECT_TRUE(IsBlocking(State::Block, 0.0f, 1000.0f));
	EXPECT_FALSE(IsBlocking(State::BlockStart, 400.0f, 1000.0f));
	EXPECT_TRUE(IsBlocking(State::BlockStart, 600.0f, 1000.0f));
	EXPECT_TRUE(IsBlocking(State::BlockEnd, 400.0f, 1000.0f));
	EXPECT_FALSE(IsBlocking(State::BlockEnd, 600.0f, 1000.0f));
	EXPECT_FLOAT_EQ(PlaybackSpeed(animations::k_StepBack, 1.0f), 1.1f * 0.9f);
	EXPECT_FLOAT_EQ(PlaybackSpeed(animations::k_AttackHigh, 1.5f), 1.1f * 0.6f * 1.5f);

	Fighter fighter;
	Enter(fighter, State::Action, animations::k_AttackSpecial, 1.5f);
	EXPECT_TRUE(fighter.special);
	EXPECT_EQ(fighter.animation, animations::k_AttackSpecial);
	// The second special move does not double its blow
	Enter(fighter, State::Action, animations::k_AttackSpecial2, 1.5f);
	EXPECT_FALSE(fighter.special);
	Enter(fighter, State::Stance);
	EXPECT_FALSE(fighter.special);
	EXPECT_EQ(fighter.animation, animations::k_Stance);
}

TEST(CreatureFight, ComputerKeepsTheGameTiers)
{
	EXPECT_EQ(TierOf(0.33f), 2u);
	EXPECT_EQ(TierOf(0.1f), 0u);
	EXPECT_EQ(TierOf(-0.9f), 0u);
}

TEST(CreatureFight, ComputerChoosesByWhatTheOpponentDoes)
{
	// A waiting opponent: the top tier always attacks, the bottom one turn in sixteen
	EXPECT_EQ(ChooseMove(2, Opponent::Stance, Always(5)).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Always(5)).kind, Choice::Kind::None);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Sequence({0, 1})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::Stance, Sequence({0, 1})).band, Band::Mid);
	// Reeling in a block, or doing anything else, the opponent is attacked
	EXPECT_EQ(ChooseMove(0, Opponent::BlockRecoil, Always(0)).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(0, Opponent::OtherAction, Always(0)).kind, Choice::Kind::Blow);
	// Attacked, the bottom tier blocks a quarter of the time, else steps away
	EXPECT_EQ(ChooseMove(0, Opponent::Attacking, Sequence({0})).kind, Choice::Kind::Block);
	const auto away = ChooseMove(0, Opponent::Attacking, Sequence({1, 2}));
	EXPECT_EQ(away.kind, Choice::Kind::Step);
	EXPECT_EQ(away.step, Step::Back);
	// The top tier counter-attacks four times in five
	EXPECT_EQ(ChooseMove(2, Opponent::Attacking, Sequence({1, 0})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(2, Opponent::Attacking, Sequence({0, 0})).kind, Choice::Kind::Block);
	// Stepping in: block, step, step or attack
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({0})).kind, Choice::Kind::Block);
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({1, 0})).kind, Choice::Kind::Step);
	EXPECT_EQ(ChooseMove(0, Opponent::SteppingForward, Sequence({3, 0})).kind, Choice::Kind::Blow);
	// Blocking: mostly waits, one turn in ten a step or a blow
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({0})).kind, Choice::Kind::None);
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({1, 0})).kind, Choice::Kind::Step);
	EXPECT_EQ(ChooseMove(0, Opponent::Blocking, Sequence({1, 1})).kind, Choice::Kind::Blow);
	EXPECT_EQ(ChooseMove(2, Opponent::Other, Always(0)).kind, Choice::Kind::None);
	// The top tier draws its lot to strike back only against a blow, a step in or a block: the band is the next draw
	EXPECT_EQ(ChooseMove(2, Opponent::Stance, Sequence({0, 1})).band, Band::Mid);
	EXPECT_EQ(ChooseMove(2, Opponent::OtherAction, Sequence({2})).band, Band::Low);
	EXPECT_EQ(ChooseMove(2, Opponent::BlockRecoil, Sequence({1})).band, Band::Mid);
	EXPECT_EQ(ChooseMove(2, Opponent::Attacking, Sequence({1, 2})).band, Band::Low);
	EXPECT_TRUE(ComputerEndsBlock(Always(1)));
	EXPECT_FALSE(ComputerEndsBlock(Always(0)));
}

TEST(CreatureFight, OpponentStatesAsTheComputerSeesThem)
{
	EXPECT_EQ(OpponentOf(State::Action, animations::k_AttackKnee), Opponent::Attacking);
	EXPECT_EQ(OpponentOf(State::Action, animations::k_StepForward), Opponent::SteppingForward);
	EXPECT_EQ(OpponentOf(State::Action, animations::k_RecoilMid), Opponent::OtherAction);
	EXPECT_EQ(OpponentOf(State::BlockEnd, animations::k_EndBlock), Opponent::Blocking);
	// A cast is struck at once, as any action that is not a blow or a step in
	EXPECT_EQ(OpponentOf(State::CastStart, animations::k_StartCast), Opponent::OtherAction);
	EXPECT_EQ(OpponentOf(State::Cast, animations::k_Cast), Opponent::OtherAction);
	EXPECT_EQ(OpponentOf(State::CastEnd, animations::k_EndCast), Opponent::EndingCast);
	EXPECT_EQ(OpponentOf(State::Start, animations::k_Start), Opponent::Other);
}

TEST(CreatureFight, ARayMeetsTheBodyWhereItsSurfaceIs)
{
	// An upright capsule of radius 1 standing 10 ahead
	const std::array<creature_feedback::Capsule, 1> body {
	    {{.from = {10.0f, 0.0f, 0.0f}, .to = {10.0f, 5.0f, 0.0f}, .radius = 1.0f}}};
	const glm::vec3 ahead {1.0f, 0.0f, 0.0f};
	const auto entry = creature_feedback::EntryAlong({0.0f, 2.0f, 0.0f}, ahead, body, 20.0f);
	ASSERT_TRUE(entry.has_value());
	EXPECT_NEAR(*entry, 9.0f, 1e-3f);
	// Starting inside it meets it at once; passing beside it, or too short, it meets nothing
	EXPECT_EQ(creature_feedback::EntryAlong({10.0f, 2.0f, 0.0f}, ahead, body, 20.0f), std::optional(0.0f));
	EXPECT_FALSE(creature_feedback::EntryAlong({0.0f, 2.0f, 3.0f}, ahead, body, 20.0f).has_value());
	EXPECT_FALSE(creature_feedback::EntryAlong({0.0f, 2.0f, 0.0f}, ahead, body, 5.0f).has_value());
	EXPECT_FALSE(creature_feedback::EntryAlong({0.0f, 2.0f, 0.0f}, ahead, {}, 20.0f).has_value());
}

TEST(CreatureFight, EachBlowIsMeasuredByItsOwnGapAndOneThatMeetsNothingIsNotUsed)
{
	const std::array<Reach, 2> reaches {{
	    {.animation = animations::k_AttackHigh, .reach = 12.0f, .height = 12.0f},
	    {.animation = animations::k_AttackBelly, .reach = 10.0f, .height = 7.0f},
	}};
	// The high blow meets the body near and lands; the belly blow's rays pass the body by
	const std::array<std::optional<float>, 2> gaps {6.0f, std::nullopt};
	EXPECT_EQ(ChooseAttack(reaches, Band::Mid, gaps, 4.0f, 1.0f, 15.0f).animation, animations::k_AttackHigh);
	// The belly blow far off: it steps in for it, as its band beats the high blow's
	const std::array<std::optional<float>, 2> far {6.0f, 18.0f};
	EXPECT_EQ(ChooseAttack(reaches, Band::Mid, far, 4.0f, 1.0f, 15.0f).step, Step::Forward);
	// None meets the body: nothing is done
	const std::array<std::optional<float>, 2> none {std::nullopt, std::nullopt};
	const auto choice = ChooseAttack(reaches, Band::Mid, none, 4.0f, 1.0f, 15.0f);
	EXPECT_FALSE(choice.animation.has_value());
	EXPECT_FALSE(choice.step.has_value());
}

TEST(CreatureFight, InItsStanceAFighterLooksAtItsOpponentsHeadAndAheadInAnyOtherMove)
{
	const glm::vec3 head {10.0f, 12.0f, -4.0f};
	EXPECT_EQ(LookAt(State::Stance, head), std::optional(head));
	for (const auto state : {State::Block, State::BlockStart, State::Action, State::BlockRecoil, State::Cast})
	{
		EXPECT_FALSE(LookAt(state, head).has_value());
	}
}

TEST(CreatureFight, ABlowIsChosenThatLandsAtTheBand)
{
	// Reaches 12 ahead at the head, 10 at the belly, 8 at the knee
	const std::array<Reach, 4> reaches {{
	    {.animation = animations::k_AttackHigh, .reach = 12.0f, .height = 12.0f},
	    {.animation = animations::k_AttackBelly, .reach = 10.0f, .height = 7.0f},
	    {.animation = animations::k_AttackKnee, .reach = 8.0f, .height = 3.0f},
	    {.animation = animations::k_AttackSpecial, .reach = 30.0f, .height = 7.0f},
	}};
	// In reach of everything, the band asked for wins
	EXPECT_EQ(ChooseAttack(reaches, Band::Mid, 6.0f, 4.0f, 1.0f, 15.0f).animation, animations::k_AttackBelly);
	EXPECT_EQ(ChooseAttack(reaches, Band::Low, 6.0f, 4.0f, 1.0f, 15.0f).animation, animations::k_AttackKnee);
	EXPECT_EQ(ChooseAttack(reaches, Band::High, 6.0f, 4.0f, 1.0f, 15.0f).animation, animations::k_AttackHigh);
	// Out of reach, it steps forward first
	const auto far = ChooseAttack(reaches, Band::Low, 20.0f, 4.0f, 1.0f, 15.0f);
	EXPECT_FALSE(far.animation.has_value());
	EXPECT_EQ(far.step, Step::Forward);
	// Too close for the knee to land well, it steps back
	EXPECT_EQ(ChooseAttack(reaches, Band::Low, -2.0f, 4.0f, 1.0f, 15.0f).step, Step::Back);
	// A blow too high for a small opponent isn't used; with no blow at all, or none that lands from ten steps, nothing
	// is done
	EXPECT_EQ(ChooseAttack(reaches, Band::High, 6.0f, 4.0f, 1.0f, 10.0f).animation, animations::k_AttackBelly);
	const auto none = ChooseAttack({}, Band::High, 6.0f, 4.0f, 1.0f, 10.0f);
	EXPECT_FALSE(none.animation.has_value());
	EXPECT_FALSE(none.step.has_value());
	const auto tooFar = ChooseAttack(reaches, Band::Low, 60.0f, 4.0f, 1.0f, 15.0f);
	EXPECT_FALSE(tooFar.animation.has_value());
	EXPECT_FALSE(tooFar.step.has_value());
}

TEST(CreatureFight, OutcomesAndStarting)
{
	EXPECT_TRUE(WinnerPoos(-0.6f));
	EXPECT_FALSE(WinnerPoos(-0.4f));
	EXPECT_EQ(Response(0.8f, 0.5f), animations::k_Happy);
	EXPECT_EQ(Response(0.4f, 0.5f), animations::k_Sad);
	EXPECT_TRUE(WantsToFight(0.7f, 1.0f, 50.0f, 1.0f, 200.0f));
	EXPECT_FALSE(WantsToFight(0.5f, 1.0f, 50.0f, 1.0f, 200.0f));
	EXPECT_FALSE(WantsToFight(0.7f, 0.05f, 50.0f, 1.0f, 200.0f));
	EXPECT_FALSE(WantsToFight(0.7f, 1.0f, 70.0f, 1.0f, 200.0f));
	EXPECT_FALSE(WantsToFight(0.7f, 1.0f, 50.0f, 1.0f, 30.0f));
}

TEST(CreatureFight, AFightCalledOffWindsDownAndFinishes)
{
	// At once from the stance, a block and the casting pose
	EXPECT_EQ(StopTransition(State::Stance, false), State::Finish);
	EXPECT_EQ(StopTransition(State::Block, false), State::BlockEnd);
	EXPECT_EQ(StopTransition(State::Cast, false), State::CastEnd);
	// The move under way plays out first
	EXPECT_FALSE(StopTransition(State::Action, false).has_value());
	EXPECT_EQ(StopTransition(State::Action, true), State::Finish);
	EXPECT_EQ(StopTransition(State::Start, true), State::Finish);
	EXPECT_EQ(StopTransition(State::BlockStart, true), State::BlockEnd);
	EXPECT_EQ(StopTransition(State::BlockRecoil, true), State::BlockEnd);
	// A cast started is never cast: it goes straight to ending the cast
	EXPECT_FALSE(StopTransition(State::CastStart, false).has_value());
	EXPECT_EQ(StopTransition(State::CastStart, true), State::CastEnd);
	// Ending a block or a cast carries on to the stance, then the finish
	EXPECT_FALSE(StopTransition(State::BlockEnd, true).has_value());
	EXPECT_FALSE(StopTransition(State::CastEnd, true).has_value());
	EXPECT_FALSE(StopTransition(State::Finish, true).has_value());
}

TEST(CreatureFight, ACreatureAgreesToFightOnlyOneOfASizeAndControlledAsItIs)
{
	// From half to twice the asker's height, either way round
	EXPECT_TRUE(AgreesToFight(5.0f, 10.0f, false, false, false));
	EXPECT_TRUE(AgreesToFight(20.0f, 10.0f, false, false, false));
	EXPECT_FALSE(AgreesToFight(4.9f, 10.0f, false, false, false));
	EXPECT_FALSE(AgreesToFight(20.1f, 10.0f, false, false, false));
	// Not while it lies dead
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, true, false, false));
	// Only when both or neither are under a script's control
	EXPECT_TRUE(AgreesToFight(10.0f, 10.0f, false, true, true));
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, false, true, false));
	EXPECT_FALSE(AgreesToFight(10.0f, 10.0f, false, false, true));
}

TEST(CreatureFight, WhatTheCameraDoesAsAFightStarts)
{
	// Already looking at the arena, it watches any fight
	EXPECT_EQ(ViewAtFightStart(false, false, true), FightView::Watch);
	EXPECT_EQ(ViewAtFightStart(true, false, true), FightView::Watch);
	// A script-controlled fighter brings it to the player's creature's fight from afar
	EXPECT_EQ(ViewAtFightStart(true, true, false), FightView::Watch);
	// but not to another god's fight
	EXPECT_EQ(ViewAtFightStart(false, true, false), FightView::None);
	// From afar, the player's creature's fight only has the spirits remark on it
	EXPECT_EQ(ViewAtFightStart(true, false, false), FightView::Remark);
	EXPECT_EQ(ViewAtFightStart(false, false, false), FightView::None);
}

TEST(CreatureFight, TheCameraIsToldAFightIsOverOnlyFromTheMiddleOfIt)
{
	for (const auto state : {State::Start, State::Stance, State::Action, State::BlockStart, State::Block, State::BlockEnd,
	                         State::CastStart, State::Cast, State::CastEnd})
	{
		EXPECT_TRUE(CameraLingersOnEnd(state)) << Name(state);
	}
	for (const auto state : {State::Idle, State::Finish, State::BlockRecoil, State::Faint, State::GetUp, State::Lying})
	{
		EXPECT_FALSE(CameraLingersOnEnd(state)) << Name(state);
	}
}

TEST(CreatureFightHud, AtTheDefaultScreenThePanelIsFlushInTheCorner)
{
	const auto layout = creature_fight_hud::Compute({800, 600});
	EXPECT_EQ(layout.unit, 8);
	EXPECT_EQ(layout.box.min, glm::vec2(0.0f, 0.0f));
	EXPECT_EQ(layout.box.max, glm::vec2(264.0f, 72.0f));
	EXPECT_FLOAT_EQ(layout.textSize, 24.0f);
	EXPECT_FLOAT_EQ(layout.nameWidth, 240.0f);
	// Health over the lower stamina bar, the name to their right
	EXPECT_EQ(layout.rows[0].health.min, glm::vec2(8.0f, 8.0f));
	EXPECT_EQ(layout.rows[0].health.max, glm::vec2(208.0f, 24.0f));
	EXPECT_EQ(layout.rows[0].stamina.min, glm::vec2(8.0f, 20.0f));
	EXPECT_EQ(layout.rows[0].stamina.max, glm::vec2(208.0f, 32.0f));
	EXPECT_EQ(layout.rows[0].name, glm::vec2(216.0f, 8.0f));
	EXPECT_EQ(layout.rows[1].health.min, glm::vec2(8.0f, 36.0f));
	EXPECT_EQ(layout.rows[1].stamina.max, glm::vec2(208.0f, 60.0f));
	EXPECT_EQ(layout.rows[1].name, glm::vec2(216.0f, 36.0f));
}

TEST(CreatureFightHud, ThePanelGrowsWithTheScreensHeightOnly)
{
	// A seventieth of the height in whole pixels, the same on a 4:3 screen and a wide one as tall
	const auto layout = creature_fight_hud::Compute({1440, 1080});
	const auto wide = creature_fight_hud::Compute({3440, 1080});
	EXPECT_EQ(layout.unit, 15);
	EXPECT_EQ(layout.box.max, glm::vec2(495.0f, 135.0f));
	EXPECT_EQ(wide.box.max, layout.box.max);
	// Three and a half units between the rows, cut to whole pixels; one and a half down to the stamina bar
	EXPECT_FLOAT_EQ(layout.rows[1].health.min.y, 67.0f);
	EXPECT_FLOAT_EQ(layout.rows[0].stamina.min.y, 15.0f + 22.0f);
}

TEST(CreatureFightHud, BarColours)
{
	EXPECT_EQ(creature_fight_hud::HealthColour(0.0f), glm::u8vec3(255, 0, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.25f), glm::u8vec3(255, 127, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.5f), glm::u8vec3(255, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(0.75f), glm::u8vec3(127, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(1.0f), glm::u8vec3(0, 255, 0));
	EXPECT_EQ(creature_fight_hud::HealthColour(2.0f), glm::u8vec3(0, 255, 0));
	EXPECT_EQ(creature_fight_hud::StaminaColour(0.0f), glm::u8vec3(32, 128, 255));
	EXPECT_EQ(creature_fight_hud::StaminaColour(1.0f), glm::u8vec3(127, 128, 255));
	EXPECT_EQ(creature_fight_hud::StaminaColour(0.5f), glm::u8vec3(79, 128, 255));
}

TEST(CreatureFightHud, ALongNameIsCutToFit)
{
	const auto tenEach = [](std::u16string_view text) { return 10.0f * static_cast<float>(text.size()); };
	EXPECT_EQ(creature_fight_hud::FitName(u"Bartholomew", 50.0f, tenEach), u"Barth");
	EXPECT_EQ(creature_fight_hud::FitName(u"Rex", 50.0f, tenEach), u"Rex");
	EXPECT_EQ(creature_fight_hud::FitName(u"", 50.0f, tenEach), u"");
}

// A landed blow is a hit of half its impact times 0.05 from the attacker, which changes how nice the victim finds it by
// that times the alignment table's weight for a hit on a creature
TEST(CreatureFight, ALandedBlowChangesHowNiceTheVictimFindsItsAttackerByTheHitsWeight)
{
	EXPECT_FLOAT_EQ(k_BlowHit, 0.025f);
	EXPECT_FLOAT_EQ(BlowNicenessChange(-2.0f), -0.05f);
	EXPECT_FLOAT_EQ(BlowNicenessChange(0.0f), 0.0f);
}

// A fighter's reach ahead is its furthest bone ahead of where it stands, and none when every bone is behind it
TEST(CreatureFight, TheForwardExtentIsTheFurthestBoneAheadAndNeverBelowNone)
{
	const std::array<float, 4> bones {1.5f, -2.0f, 3.25f, 0.5f};
	EXPECT_FLOAT_EQ(ForwardExtent(bones), 3.25f);
	const std::array<float, 2> behind {-1.0f, -0.5f};
	EXPECT_FLOAT_EQ(ForwardExtent(behind), 0.0f);
	EXPECT_FLOAT_EQ(ForwardExtent({}), 0.0f);
}

// In a blow's turn the two are pushed apart along the attacker's facing, by half of how much nearer they stand than the
// attacker's reach ahead and half the opponent's; far enough apart, nothing moves
TEST(CreatureFight, ABlowPushesTheTwoApartAlongTheAttackersFacingByHalfTheOverlap)
{
	const glm::vec2 ahead {0.0f, -1.0f};
	// 4 apart against a reach of 3 + 4 / 2 = 5: each moves 0.5, the opponent on along the facing
	const auto push = BlowPushOf({0.0f, 0.0f}, {0.0f, -4.0f}, ahead, 3.0f, 4.0f);
	ASSERT_TRUE(push.has_value());
	EXPECT_FLOAT_EQ(push->x, 0.0f);
	EXPECT_FLOAT_EQ(push->y, -0.5f);
	// along the facing even when the opponent stands to one side
	const auto aside = BlowPushOf({0.0f, 0.0f}, {3.0f, 0.0f}, ahead, 3.0f, 4.0f);
	ASSERT_TRUE(aside.has_value());
	EXPECT_FLOAT_EQ(aside->y, -1.0f);
	EXPECT_FALSE(BlowPushOf({0.0f, 0.0f}, {0.0f, -5.0f}, ahead, 3.0f, 4.0f).has_value());
	EXPECT_FALSE(BlowPushOf({0.0f, 0.0f}, {0.0f, -6.0f}, ahead, 3.0f, 4.0f).has_value());
}
