/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/Scaffolds.h"

using openblack::ecs::scaffolds::PhantomSoundMarks;
using openblack::ecs::scaffolds::PhantomSoundOf;

TEST(ScaffoldPhantomSounds, TheAppearSoundPlaysOncePerFadeInPitchedByTheBuilding)
{
	PhantomSoundMarks marks;
	// The timer 400 with 30 ms into the turn: 370 left, fading in, abode number 4
	const auto first = PhantomSoundOf(marks, 400, 1230, true, 4);
	ASSERT_TRUE(first.has_value());
	EXPECT_EQ(first->sample, 51);
	EXPECT_EQ(first->mode, 3);
	EXPECT_EQ(first->pitch, 110 - 3 * 4);
	// The same phase: nothing more, however many frames
	EXPECT_FALSE(PhantomSoundOf(marks, 300, 1240, true, 4).has_value());
	// No building number: pitch 100
	PhantomSoundMarks fresh;
	EXPECT_EQ(PhantomSoundOf(fresh, 400, 0, true, -1)->pitch, 100);
}

TEST(ScaffoldPhantomSounds, FadingOutPlaysTheDisappearSoundOnceAndLetsTheAppearSoundPlayAgain)
{
	PhantomSoundMarks marks;
	ASSERT_TRUE(PhantomSoundOf(marks, 400, 0, true, 0).has_value());
	const auto out = PhantomSoundOf(marks, 400, 0, false, 0);
	ASSERT_TRUE(out.has_value());
	EXPECT_EQ(out->sample, 50);
	EXPECT_EQ(out->mode, 2);
	EXPECT_EQ(out->pitch, 100);
	EXPECT_FALSE(PhantomSoundOf(marks, 400, 0, false, 0).has_value());
	// A new fade-in sounds again
	EXPECT_TRUE(PhantomSoundOf(marks, 400, 0, true, 0).has_value());
}

TEST(ScaffoldPhantomSounds, NothingOnceTheTimerIsNoMoreThanThePartOfTheTurn)
{
	PhantomSoundMarks marks;
	// 70 ms into the turn and 70 left on the timer: nothing, and the marks stay as they were
	EXPECT_FALSE(PhantomSoundOf(marks, 70, 570, true, 0).has_value());
	EXPECT_FALSE(PhantomSoundOf(marks, 0, 0, true, 0).has_value());
	EXPECT_FALSE(marks.appearPlayed);
	// A timer below zero, compared unsigned, keeps the part of the turn and is still not above it
	EXPECT_FALSE(PhantomSoundOf(marks, -50, 10, true, 0).has_value());
	EXPECT_FALSE(marks.appearPlayed);
	// One ms above the part of the turn sounds
	EXPECT_TRUE(PhantomSoundOf(marks, 71, 570, true, 0).has_value());
}
