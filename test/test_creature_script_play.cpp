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
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	for (size_t i = 0; i < 3; ++i)
	{
		EXPECT_EQ(agenda->at(i).kind, creature_mind::Step::Kind::Action);
		EXPECT_EQ(agenda->at(i).animation, 63);
	}
	EXPECT_EQ(agenda->back().kind, creature_mind::Step::Kind::WaitUntilFree);
	// None of its steps has a look of its own, so the head looks about throughout: a gaze that names nothing
	for (const auto& step : *agenda)
	{
		ASSERT_TRUE(step.gaze.has_value());
		EXPECT_FALSE(step.gaze->object.has_value() || step.gaze->point.has_value() || step.gaze->camera);
	}
	// Asked to play it no times, it only waits for its body to be free
	const auto none = Agenda({.animation = 72, .plays = 0});
	ASSERT_TRUE(none.has_value());
	ASSERT_EQ(none->size(), 1);
	EXPECT_EQ(none->front().kind, creature_mind::Step::Kind::WaitUntilFree);
}

// The wait until the body is free looks at the body from the turn it starts, busy or not, and is done on the turn after
// it first sees it free: a play is over two turns after its last action has played through
TEST(CreatureScriptPlay, TheWaitUntilTheBodyIsFreeEndsATurnAfterItIsSeenFree)
{
	creature_mind::IdleMind mind;
	creature_mind::Plan(mind, creature_mind::Activity::Told, *Agenda({.animation = 63, .plays = 1}));
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
	creature_mind::Plan(mind, creature_mind::Activity::Told, *Agenda({.animation = 63, .plays = 0}));
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
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 3);
	for (size_t i = 0; i < 2; ++i)
	{
		const auto& step = agenda->at(i);
		EXPECT_EQ(step.kind, creature_mind::Step::Kind::Static);
		EXPECT_EQ(step.sequence, (std::array<size_t, 3> {37, 38, 39}));
		EXPECT_FLOAT_EQ(step.seconds, k_StaticActionSeconds);
	}
	EXPECT_EQ(agenda->back().kind, creature_mind::Step::Kind::WaitUntilFree);
	EXPECT_EQ(Agenda({.animation = 51, .plays = 1})->front().kind, creature_mind::Step::Kind::Static);
	EXPECT_EQ(Agenda({.animation = 52, .plays = 1})->front().kind, creature_mind::Step::Kind::Action);
	EXPECT_EQ(Agenda({.animation = 0xFFFFFFFFu, .plays = 1})->front().kind, creature_mind::Step::Kind::Action);
	// The static action 0 has no start before it
	EXPECT_FALSE(Agenda({.animation = 0, .plays = 1}).has_value());
}

// A creature has played once its agenda is over, or while it only idles; it hasn't while any agenda goes on
TEST(CreatureScriptPlay, PlayedOnceTheAgendaIsOver)
{
	creature_mind::IdleMind mind;
	EXPECT_TRUE(Played(mind));
	mind.activity = creature_mind::Activity::Told;
	mind.agenda = *Agenda({.animation = 63, .plays = 2});
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
