/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' fade and the cinema bars

#include <gtest/gtest.h>

#include "Gui/CinemaBars.h"
#include "Gui/ScriptFade.h"

using openblack::gui::CinemaBars;
using openblack::gui::ScriptFade;

TEST(ScriptFade, FadesToAColourOverTurns)
{
	ScriptFade fade;
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0u);

	// A second is ten turns of 25.5 each
	fade.FadeTo(0x10, 0x20, 0x30, 1.0f);
	EXPECT_FALSE(fade.IsFinished());
	fade.ProcessTurn();
	EXPECT_EQ(fade.GetColour(), 0x19102030u);
	for (int i = 0; i < 9; ++i)
	{
		fade.ProcessTurn();
	}
	EXPECT_EQ(fade.GetColour() >> 24u, 255u);
	EXPECT_FALSE(fade.IsFinished());
	// It stops on the turn it would pass the end
	fade.ProcessTurn();
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0xFF102030u);
}

TEST(ScriptFade, TheTimeIsTruncatedToWholeSeconds)
{
	// 1.9 seconds is 1: ten turns
	ScriptFade fade;
	fade.FadeTo(0, 0, 0, 1.9f);
	for (int i = 0; i < 10; ++i)
	{
		fade.ProcessTurn();
	}
	EXPECT_FALSE(fade.IsFinished());
	fade.ProcessTurn();
	EXPECT_TRUE(fade.IsFinished());
	// under a second is none: at once
	fade.FadeBackToNormal(0.9f);
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour() >> 24u, 0u);
}

TEST(ScriptFade, NoTimeIsAtOnce)
{
	ScriptFade fade;
	fade.FadeTo(0xFF, 0, 0, 0.0f);
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0xFFFF0000u);
	fade.FadeBackToNormal(-1.0f);
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour(), 0x00FF0000u);
}

TEST(ScriptFade, FadesBackToClear)
{
	ScriptFade fade;
	fade.FadeTo(0xFF, 0xFF, 0xFF, 0.0f);
	fade.FadeBackToNormal(2.0f);
	fade.ProcessTurn();
	// 255 less 12.75, rounded down
	EXPECT_EQ(fade.GetColour() >> 24u, 242u);
	for (int i = 0; i < 20; ++i)
	{
		fade.ProcessTurn();
	}
	EXPECT_TRUE(fade.IsFinished());
	EXPECT_EQ(fade.GetColour() >> 24u, 0u);
}

TEST(ScriptFade, ASetColourKeepsTheFadeGoing)
{
	ScriptFade fade;
	fade.FadeTo(0, 0, 0, 1.0f);
	fade.SetColour(0x80FFFFFFu);
	EXPECT_EQ(fade.GetColour(), 0x80FFFFFFu);
	EXPECT_FALSE(fade.IsFinished());
	// the next turn writes its own alpha over the colour set
	fade.ProcessTurn();
	EXPECT_EQ(fade.GetColour(), 0x19FFFFFFu);
}

TEST(CinemaBars, ANewGameStartsWithThemSlidingOut)
{
	CinemaBars bars;
	bars.Update(0.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 1.0f);
	bars.Update(1000.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.5f);
	bars.Update(1000.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
	EXPECT_TRUE(bars.IsTransitionFinished());
}

TEST(CinemaBars, SlideInAndTurnBackFromWhereTheyAre)
{
	CinemaBars bars;
	bars.Update(5000.0f);
	bars.Set(true, 2.0f);
	EXPECT_TRUE(bars.IsOn());
	bars.Update(500.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.25f);
	EXPECT_FALSE(bars.IsTransitionFinished());
	// Out again from a quarter in
	bars.Set(false, 2.0f);
	bars.Update(0.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.25f);
	bars.Update(500.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.0f);
}

TEST(CinemaBars, SettingTheSameAgainChangesNothing)
{
	CinemaBars bars;
	bars.Update(5000.0f);
	bars.Set(true, 2.0f);
	bars.Update(1000.0f);
	// the second is ignored with its 4 s, which would make it a quarter
	bars.Set(true, 4.0f);
	bars.Update(0.0f);
	EXPECT_FLOAT_EQ(bars.GetFraction(), 0.5f);
}

TEST(CinemaBars, BarsMakeThePictureSixteenByNine)
{
	// 768 - 576 is 192, 96 a bar
	EXPECT_EQ(CinemaBars::BarHeight(1024, 768, 1.0f), 96);
	EXPECT_EQ(CinemaBars::BarHeight(1024, 768, 0.5f), 48);
	EXPECT_EQ(CinemaBars::BarHeight(1280, 720, 1.0f), 0);
	EXPECT_EQ(CinemaBars::BarHeight(2560, 1080, 1.0f), 0);
}
