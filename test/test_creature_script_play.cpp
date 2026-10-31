/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>

#include <gtest/gtest.h>

#include "Creature/CreatureScriptPlay.h"

using namespace openblack;
using namespace openblack::creature_script_play;

// "CreatureCow play 63 loop 1" and the like: the individual action as many times as asked, one after another, then a
// wait until the body is free
TEST(CreatureScriptPlay, AnIndividualActionIsPlayedAsManyTimesAsAsked)
{
	const auto agenda = Agenda({.animation = 63, .plays = 3});
	ASSERT_EQ(agenda.size(), 4);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(agenda.at(i).kind, creature_mind::Step::Kind::Action);
		EXPECT_EQ(agenda.at(i).animation, 63);
	}
	EXPECT_EQ(agenda.back().kind, creature_mind::Step::Kind::WaitUntilFree);
	// None of its steps has a look of its own, so the head looks about throughout: a gaze that names nothing
	for (const auto& step : agenda)
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_FALSE(step.gaze->object.has_value() || step.gaze->point.has_value() || step.gaze->camera);
	}
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 72, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// A script's sit (the static action 38) counts as a sit, which rests the body each turn, from the turn after it starts
// to the turn its end is asked for, that turn included: a hundred turns at the game's ten turns a second. Another static
// action does not
TEST(CreatureScriptPlay, AScriptsSitRestsTheBodyEachTurnItIsHeld)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 38, .plays = 1}));
	const auto never = [](uint32_t) { return 0u; };
	creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10};
	// not yet started, then the turn it starts: not counting
	EXPECT_FALSE(creature_mind::SitCounting(mind));
	ASSERT_TRUE(creature_mind::Think(mind, senses, never).startSequence.has_value());
	senses.bodyBusy = true;
	senses.bodyLooping = true;
	uint32_t counted = 0;
	bool ended = false;
	for (int turn = 0; turn < 200 && !ended; ++turn)
	{
		const bool counting = creature_mind::SitCounting(mind);
		counted += counting ? 1 : 0;
		ended = creature_mind::Think(mind, senses, never).endSit;
		EXPECT_TRUE(counting) << turn;
	}
	ASSERT_TRUE(ended);
	EXPECT_EQ(counted, 100u);
	// its end playing, it no longer counts
	EXPECT_FALSE(creature_mind::SitCounting(mind));
	// the Guide's static action 47 is not a sit
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 47, .plays = 1}));
	senses.bodyBusy = false;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_TRUE(mind.stepStarted);
	EXPECT_FALSE(creature_mind::SitCounting(mind));
}

// The wait until the body is free looks at the body from the turn it starts, busy or not, and is done on the turn after
// it first sees it free: a play is over two turns after its last action has played through
TEST(CreatureScriptPlay, TheWaitUntilTheBodyIsFreeEndsATurnAfterItIsSeenFree)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 1}));
	creature_mind::Senses senses {.seconds = 0.1f};
	const auto never = [](uint32_t) { return 0u; };
	// the action starts, plays, and is done on the first turn the body is free
	ASSERT_TRUE(creature_mind::Think(mind, senses, never).playOnce.has_value());
	senses.bodyBusy = true;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	senses.bodyBusy = false;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(Played(mind));
	// the wait starts and sees the body free; the turn after, it is done
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 1u);
	EXPECT_FALSE(Played(mind));
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 2u);
	EXPECT_TRUE(Played(mind));
	// a body busy as it starts keeps it waiting: done the turn after the body is first seen free
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 0}));
	senses.bodyBusy = true;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_TRUE(mind.stepStarted);
	static_cast<void>(creature_mind::Think(mind, senses, never));
	senses.bodyBusy = false;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 0u);
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 1u);
}

// Numbers up to 51 are static actions: the number is the loop, held ten seconds, with the start before it and the end
// after it, as many times as asked; from 52 on (any number the game reads as 52 or more, unsigned) they are individual
TEST(CreatureScriptPlay, AStaticActionIsItsStartLoopAndEndHeldTenSeconds)
{
	const auto agenda = Agenda({.animation = 38, .plays = 2});
	ASSERT_EQ(agenda.size(), 3);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto& step = agenda.at(i);
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Static);
		EXPECT_EQ(step.sequence, (std::array<size_t, 3> {37, 38, 39}));
		EXPECT_FLOAT_EQ(step.seconds, k_StaticActionSeconds);
	}
	EXPECT_EQ(agenda.back().kind, creature_mind::Step::Kind::WaitUntilFree);
	EXPECT_EQ(Agenda({.animation = 51, .plays = 1}).front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(Agenda({.animation = 52, .plays = 1}).front().kind, creature_mind::Step::Kind::Action);
	EXPECT_EQ(Agenda({.animation = 0xFFFFFFFFu, .plays = 1}).front().kind, creature_mind::Step::Kind::Action);
	// The static action 0 has no start before it, and the body has no clip below 0: neither ever starts
	EXPECT_TRUE(Agenda({.animation = 0, .plays = 1}).front().neverStarts);
	EXPECT_TRUE(Agenda({.animation = 0xFFFFFFFFu, .plays = 1}).front().neverStarts);
	EXPECT_FALSE(Agenda({.animation = 1, .plays = 1}).front().neverStarts);
	EXPECT_FALSE(Agenda({.animation = 0x7FFFFFFFu, .plays = 1}).front().neverStarts);
}

// The loop is held ten seconds in the game's whole turns: a thousand over the milliseconds of a turn, as an integer,
// times ten, counted as it starts and one off each turn after, its end asked for on the turn the count runs out. The
// seconds of the turns are not added up
TEST(CreatureScriptPlay, AStaticActionIsHeldItsSecondsInWholeTurns)
{
	EXPECT_EQ(creature_mind::HeldTurns(k_StaticActionSeconds, 1000 / 100), 100);
	EXPECT_EQ(creature_mind::HeldTurns(k_StaticActionSeconds, 1000 / 333), 30);
	EXPECT_EQ(creature_mind::HeldTurns(k_StaticActionSeconds, 1000 / 1500), 0);
	// a count of none goes round its 16 bits
	uint16_t none = 0;
	EXPECT_FALSE(creature_mind::CountHeldTurn(none));
	EXPECT_EQ(none, 65535);

	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 47, .plays = 1}));
	ASSERT_TRUE(mind.agenda.front().heldInTurns);
	const auto never = [](uint32_t) { return 0u; };
	// turns of half the game's seconds: summed, they would take 200 turns to reach ten seconds
	creature_mind::Senses senses {.seconds = 0.05f, .turnsPerSecond = 10};
	ASSERT_TRUE(creature_mind::Think(mind, senses, never).startSequence.has_value());
	EXPECT_EQ(mind.heldTurns, 100);
	senses.bodyBusy = true;
	senses.bodyLooping = true;
	for (int turn = 1; turn < 100; ++turn)
	{
		ASSERT_FALSE(creature_mind::Think(mind, senses, never).endSit) << turn;
	}
	EXPECT_TRUE(creature_mind::Think(mind, senses, never).endSit);
	// its end playing, the count is left alone
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.heldTurns, 0);
	senses.bodyBusy = false;
	senses.bodyLooping = false;
	static_cast<void>(creature_mind::Think(mind, senses, never));
	EXPECT_EQ(mind.step, 1u);
}

// A creature has played once its agenda is over, or while it only idles; it hasn't while any agenda goes on
TEST(CreatureScriptPlay, PlayedOnceTheAgendaIsOver)
{
	creature_mind::IdleMind mind;
	EXPECT_TRUE(Played(mind));
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = Agenda({.animation = 63, .plays = 2});
	mind.step = 1;
	EXPECT_FALSE(Played(mind));
	// the plays over, the wait until the body is free is still to come
	mind.step = 2;
	EXPECT_FALSE(Played(mind));
	mind.step = 3;
	EXPECT_TRUE(Played(mind));
	// Its own agenda counts as well
	mind.activity = creature_mind::Activity::HangAround;
	mind.step = 0;
	EXPECT_FALSE(Played(mind));
	mind.activity = creature_mind::Activity::BeIdle;
	EXPECT_TRUE(Played(mind));
}

// A static action plays left to right half the time, and draws for it on every turn it tries to start: the turns its
// body is still busy and refuses it as well as the turn it starts. Once it has started it draws no more
TEST(CreatureScriptPlay, AStaticActionDrawsForItsMirrorOnEveryTryToStart)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 47, .plays = 1}));
	ASSERT_TRUE(mind.agenda.front().mirrorAtRandom);
	uint32_t draws = 0;
	const auto mirror = [&draws](uint32_t range) {
		EXPECT_EQ(range, 2u);
		++draws;
		return 1u;
	};
	creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .bodyBusy = true};
	// three turns its body is busy: refused each time, a draw each time, and nothing played
	for (uint32_t turn = 1; turn <= 3; ++turn)
	{
		const auto commands = creature_mind::Think(mind, senses, mirror);
		EXPECT_FALSE(commands.startSequence.has_value());
		EXPECT_FALSE(mind.stepStarted);
		EXPECT_EQ(draws, turn);
	}
	// free, it starts, with a draw of its own that mirrors it
	senses.bodyBusy = false;
	const auto start = creature_mind::Think(mind, senses, mirror);
	ASSERT_TRUE(start.startSequence.has_value());
	EXPECT_TRUE(start.mirrored);
	EXPECT_EQ(draws, 4u);
	// held, it draws no more
	senses.bodyBusy = true;
	senses.bodyLooping = true;
	for (int turn = 0; turn < 10; ++turn)
	{
		static_cast<void>(creature_mind::Think(mind, senses, mirror));
	}
	EXPECT_EQ(draws, 4u);
}

// An individual action draws for its mirror on every turn it tries to start: the turns its body is still busy and
// refuses it as well as the turn it starts, and that last draw mirrors it. Once it has started it draws no more
TEST(CreatureScriptPlay, AnIndividualActionDrawsForItsMirrorOnEveryTryToStart)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 1}));
	uint32_t draws = 0;
	const auto mirror = [&draws](uint32_t range) {
		EXPECT_EQ(range, 2u);
		++draws;
		return 1u;
	};
	creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .bodyBusy = true};
	// three turns its body is busy: refused each time, a draw each time, and nothing played
	for (uint32_t turn = 1; turn <= 3; ++turn)
	{
		const auto commands = creature_mind::Think(mind, senses, mirror);
		EXPECT_FALSE(commands.playOnce.has_value());
		EXPECT_FALSE(mind.stepStarted);
		EXPECT_EQ(draws, turn);
	}
	// free, it starts, with a draw of its own that mirrors it
	senses.bodyBusy = false;
	const auto start = creature_mind::Think(mind, senses, mirror);
	ASSERT_TRUE(start.playOnce.has_value());
	EXPECT_EQ(*start.playOnce, size_t {63});
	EXPECT_TRUE(start.mirrored);
	EXPECT_EQ(draws, 4u);
	// playing, it draws no more
	senses.bodyBusy = true;
	senses.bodyPlaying = true;
	for (int turn = 0; turn < 5; ++turn)
	{
		static_cast<void>(creature_mind::Think(mind, senses, mirror));
	}
	EXPECT_EQ(draws, 4u);
}

// While it holds something an individual action draws nothing, refused or started, and is mirrored unless what it
// holds is in its left hand
TEST(CreatureScriptPlay, AnIndividualActionWhileHoldingSomethingTakesTheHoldingHandsMirror)
{
	const auto none = [](uint32_t) -> uint32_t {
		ADD_FAILURE() << "an individual action while holding something draws nothing";
		return 0u;
	};
	for (const bool leftHand : {false, true})
	{
		creature_mind::IdleMind mind;
		creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = 63, .plays = 1}));
		creature_mind::Senses senses {.seconds = 0.1f, .bodyBusy = true, .holding = true, .heldMirrored = leftHand};
		EXPECT_FALSE(creature_mind::Think(mind, senses, none).playOnce.has_value());
		senses.bodyBusy = false;
		const auto start = creature_mind::Think(mind, senses, none);
		ASSERT_TRUE(start.playOnce.has_value());
		EXPECT_EQ(start.mirrored, !leftHand);
	}
}

// The static action 0 and an individual action below 0 have no clip, so the body refuses them every turn, free or
// busy: the creature never gets past the first play and has never played. Each try draws for its side as a try to
// start draws: the static action always, the individual action only while it holds nothing, the holding hand choosing
// otherwise
TEST(CreatureScriptPlay, APlayWithNoClipIsTriedEveryTurnAndNeverStarts)
{
	for (const uint32_t animation : {0u, 0xFFFFFFFFu})
	{
		for (const bool holding : {false, true})
		{
			creature_mind::IdleMind mind;
			creature_mind::Plan(mind, creature_mind::Activity::Told, Agenda({.animation = animation, .plays = 2}));
			uint32_t draws = 0;
			const auto random = [&draws](uint32_t range) {
				EXPECT_EQ(range, 2u);
				++draws;
				return 1u;
			};
			creature_mind::Senses senses {.seconds = 0.1f, .turnsPerSecond = 10, .holding = holding};
			for (int turn = 0; turn < 6; ++turn)
			{
				senses.bodyBusy = turn % 2 == 1;
				const auto commands = creature_mind::Think(mind, senses, random);
				EXPECT_FALSE(commands.playOnce.has_value() || commands.startSequence.has_value()) << animation << turn;
			}
			EXPECT_EQ(mind.step, 0u);
			EXPECT_FALSE(mind.stepStarted);
			EXPECT_FALSE(Played(mind));
			const bool drawsEachTry = animation == 0 || !holding;
			EXPECT_EQ(draws, drawsEachTry ? 6u : 0u) << animation << " " << holding;
		}
	}
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 0, .plays = 0});
	ASSERT_EQ(none.size(), 1);
	EXPECT_EQ(none.front().kind, creature_mind::Step::Kind::WaitUntilFree);
}
