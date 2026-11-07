/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The cinematic director: the script fade moved on by the turns, the cinema bars by the frames' game time

#define LOCATOR_IMPLEMENTATIONS

#include <chrono>

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"

using openblack::ecs::systems::CinematicDirectorSystem;
using Milliseconds = std::chrono::duration<float, std::milli>;

TEST(CinematicDirector, StartsClearWithTheBarsSlidingOut)
{
	CinematicDirectorSystem director;
	EXPECT_TRUE(director.IsFadeFinished());
	EXPECT_EQ(director.GetFadeColour(), 0u);
	EXPECT_FALSE(director.IsWideScreenOn());
	EXPECT_FLOAT_EQ(director.GetWideScreenFraction(), 0.0f);
}

TEST(CinematicDirector, TheFadeMovesOnlyWithTheTurns)
{
	CinematicDirectorSystem director;
	director.FadeTo(0x10, 0x20, 0x30, 1.0f);
	director.Update(Milliseconds(1000.0f));
	EXPECT_EQ(director.GetFadeColour(), 0x00102030u);
	director.ProcessTurn();
	EXPECT_EQ(director.GetFadeColour(), 0x19102030u);
	director.FadeBackToNormal(0.0f);
	EXPECT_TRUE(director.IsFadeFinished());
	EXPECT_EQ(director.GetFadeColour(), 0x00102030u);
	director.SetFadeColour(0xFFFFFFFFu);
	EXPECT_EQ(director.GetFadeColour(), 0xFFFFFFFFu);
}

TEST(CinematicDirector, TheBarsMoveOnlyWithTheFramesGameTime)
{
	CinematicDirectorSystem director;
	director.Update(Milliseconds(5000.0f));
	director.SetWideScreen(true, 2.0f);
	EXPECT_TRUE(director.IsWideScreenOn());
	director.ProcessTurn();
	EXPECT_FLOAT_EQ(director.GetWideScreenFraction(), 0.0f);
	director.Update(Milliseconds(1000.0f));
	EXPECT_FLOAT_EQ(director.GetWideScreenFraction(), 0.5f);
	EXPECT_FALSE(director.IsWideScreenTransitionFinished());
	director.Update(Milliseconds(1000.0f));
	EXPECT_TRUE(director.IsWideScreenTransitionFinished());
}

TEST(CinematicDirector, AFilmSnapsTheBarsIn)
{
	CinematicDirectorSystem director;
	director.Update(Milliseconds(5000.0f));
	director.SetWideScreen(true, 2.0f);
	director.SnapWideScreen();
	director.Update(Milliseconds(0.0f));
	EXPECT_FLOAT_EQ(director.GetWideScreenFraction(), 1.0f);
	EXPECT_TRUE(director.IsWideScreenTransitionFinished());
}

TEST(CinematicDirector, TheStoryOpensOnABlackScreen)
{
	CinematicDirectorSystem director;
	EXPECT_EQ(director.GetFadeColour() >> 24u, 0u);

	// Black at once, before any turn, so nothing of the land shows before its scripts set the scene
	director.StartStory();
	EXPECT_EQ(director.GetFadeColour(), 0xFF000000u);
	EXPECT_TRUE(director.IsFadeFinished());
	director.ProcessTurn();
	EXPECT_EQ(director.GetFadeColour(), 0xFF000000u);

	// Until the scripts fade the picture back in
	director.FadeBackToNormal(0.0f);
	EXPECT_EQ(director.GetFadeColour() >> 24u, 0u);
}

// The first land's opening fades in from the black over twelve seconds, a step of 255 / 120 a turn
TEST(CinematicDirector, TheOpeningFadesInFromTheBlack)
{
	CinematicDirectorSystem director;
	director.StartStory();
	director.FadeBackToNormal(12.0f);
	EXPECT_FALSE(director.IsFadeFinished());
	EXPECT_EQ(director.GetFadeColour(), 0xFF000000u);
	director.ProcessTurn();
	EXPECT_EQ(director.GetFadeColour(), 0xFC000000u);
	for (int turn = 1; turn < 120; ++turn)
	{
		director.ProcessTurn();
	}
	EXPECT_EQ(director.GetFadeColour() >> 24u, 0u);
	director.ProcessTurn();
	EXPECT_TRUE(director.IsFadeFinished());
	EXPECT_EQ(director.GetFadeColour() >> 24u, 0u);
}

TEST(CinematicDirector, CloseClippingIsOffUntilAScriptAsks)
{
	CinematicDirectorSystem director;
	EXPECT_FALSE(director.IsCloseClipping());
	director.SetCloseClipping(true);
	EXPECT_TRUE(director.IsCloseClipping());
	director.SetCloseClipping(false);
	EXPECT_FALSE(director.IsCloseClipping());
}

// The scripts' restart (a new game, a playground's start) stops the fade with nothing drawn and turns the close clipping
// off; a land change does neither
TEST(CinematicDirector, TheScriptsRestartClearsTheCloseClippingAndTheFade)
{
	CinematicDirectorSystem director;
	director.SetCloseClipping(true);
	director.FadeTo(0x10, 0x20, 0x30, 2.0f);
	director.ProcessTurn();
	ASSERT_FALSE(director.IsFadeFinished());
	ASSERT_NE(director.GetFadeColour() >> 24u, 0u);

	director.OnScriptsRestart();
	EXPECT_FALSE(director.IsCloseClipping());
	EXPECT_TRUE(director.IsFadeFinished());
	EXPECT_EQ(director.GetFadeColour(), 0u);
	director.ProcessTurn();
	EXPECT_EQ(director.GetFadeColour(), 0u);
}
