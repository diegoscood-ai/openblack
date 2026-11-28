/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Common/Zoomer.h"
#include "Gui/StartupRules.h"
#include "Video/StillPicture.h"

using namespace openblack;
using namespace openblack::startup;

TEST(Startup, LogoAlphaIsTruncatedAndKeptInRange)
{
	EXPECT_EQ(LogoAlpha(0.0f), 0);
	EXPECT_EQ(LogoAlpha(-0.5f), 0);
	EXPECT_EQ(LogoAlpha(0.5f), 127);
	EXPECT_EQ(LogoAlpha(0.999f), 254);
	EXPECT_EQ(LogoAlpha(1.0f), 255);
	// The hold goes on to 2: still opaque
	EXPECT_EQ(LogoAlpha(2.0f), 255);
}

TEST(Startup, LogoStillFollowsTheZoomersMoves)
{
	// The same moves on the shared Zoomer, a frame of 16 ms at a time
	LogoStill still;
	Zoomer zoomer;
	zoomer.SetDestinationWithSpeedAndTime(1.0f, 0.0f, 0.7f);
	int32_t phase = 0;
	for (int frame = 0; frame < 400 && !still.Ended(); ++frame)
	{
		zoomer.Update(static_cast<float>(16) * 0.001f);
		ASSERT_EQ(still.Frame(16, false), LogoAlpha(zoomer.GetValue())) << "frame " << frame;
		if (zoomer.HasArrived())
		{
			++phase;
			zoomer.SetDestinationWithSpeedAndTime(phase == 1 ? 2.0f : 0.0f, 0.0f, phase == 1 ? 1.7f : 1.1f);
		}
	}
	EXPECT_TRUE(still.Ended());
	EXPECT_EQ(phase, 3);
}

TEST(Startup, PressingWhileHeldFadesOutFromOpaque)
{
	LogoStill still;
	for (int i = 0; i < 100; ++i)
	{
		still.Frame(10, false);
	}
	// Held at more than 1: the press puts it back at 1, and the fade out takes its full 1.1 s from there
	EXPECT_EQ(still.Frame(10, true), 255);
	int32_t frames = 0;
	while (!still.Ended() && frames < 1000)
	{
		still.Frame(10, false);
		++frames;
	}
	EXPECT_NEAR(frames, 110, 2);
}

TEST(Startup, PicturesAreDrawnInTilesOfTheirTextures)
{
	// The logo film, 768 by 512, over a 1024 by 768 screen from a pixel before its corner: 3 by 2 whole tiles
	const auto tiles = video::PictureTiles(768, 512, {.x = -1, .y = -1, .width = 1024, .height = 768});
	ASSERT_EQ(tiles.size(), 6u);
	EXPECT_FLOAT_EQ(tiles[0].min.x, -1.0f);
	EXPECT_FLOAT_EQ(tiles[0].min.y, -1.0f);
	EXPECT_FLOAT_EQ(tiles[0].max.x, -1.0f + 256.0f * 1024.0f / 768.0f);
	EXPECT_FLOAT_EQ(tiles[5].max.x, -1.0f + 1024.0f);
	EXPECT_FLOAT_EQ(tiles[5].max.y, -1.0f + 768.0f);
	// Half a texel inside each tile's edges
	EXPECT_FLOAT_EQ(tiles[0].uvMin.x, 0.5f / 768.0f);
	EXPECT_FLOAT_EQ(tiles[0].uvMax.x, 255.5f / 768.0f);
	EXPECT_FLOAT_EQ(tiles[1].uvMin.x, 256.5f / 768.0f);
	EXPECT_FLOAT_EQ(tiles[3].uvMin.y, 256.5f / 512.0f);
	// A picture of 640 by 360: the last column 128 texels, the last row 104
	const auto cut = video::PictureTiles(640, 360, {.x = 0, .y = 0, .width = 640, .height = 360});
	ASSERT_EQ(cut.size(), 6u);
	EXPECT_FLOAT_EQ(cut[2].max.x - cut[2].min.x, 128.0f);
	EXPECT_FLOAT_EQ(cut[5].max.y - cut[5].min.y, 104.0f);
	EXPECT_FLOAT_EQ(cut[5].uvMax.y, 359.5f / 360.0f);
	EXPECT_TRUE(video::PictureTiles(0, 10, {.x = 0, .y = 0, .width = 1, .height = 1}).empty());
}

// A start that asks for the automatic save loads it when no land was asked for, the player has a profile and it can be
// read; a new game asked for, a land asked for, a first player with no profile or a save that can't be read start a new
// game
TEST(StartupRules, TheAutomaticSaveIsLoadedOnlyWhenAskedForAndReadable)
{
	using startup::GameStart;
	EXPECT_EQ(startup::StartOf(true, false, true, true), GameStart::AutoSave);
	EXPECT_EQ(startup::StartOf(false, false, true, true), GameStart::NewGame);
	EXPECT_EQ(startup::StartOf(true, true, true, true), GameStart::NewGame);
	EXPECT_EQ(startup::StartOf(true, false, false, true), GameStart::NewGame);
	EXPECT_EQ(startup::StartOf(true, false, true, false), GameStart::NewGame);
}

// A game started from the automatic save opens on the main menu; a new game does not
TEST(StartupRules, TheSkirmishBoxsBackReloadsTheAutomaticSaveWithTheMenuOverIt)
{
	using startup::GameStart;
	// Back: the automatic save, when the player has one that can be read, and the main menu over it
	const bool back = startup::AutoSaveAskedAfterSkirmishBox(true);
	EXPECT_EQ(startup::StartOf(back, false, true, true), GameStart::AutoSave);
	EXPECT_TRUE(startup::OpensOnMainMenu(startup::StartOf(back, false, true, true)));
	// with none that can be read, a new game, on the land
	EXPECT_EQ(startup::StartOf(back, false, true, false), GameStart::NewGame);
	EXPECT_FALSE(startup::OpensOnMainMenu(startup::StartOf(back, false, true, false)));
	// Start Game, or no box to show: no automatic save
	EXPECT_EQ(startup::StartOf(startup::AutoSaveAskedAfterSkirmishBox(false), false, true, true), GameStart::NewGame);
}

TEST(StartupRules, AGameStartedFromTheAutomaticSaveOpensOnTheMainMenu)
{
	EXPECT_TRUE(startup::OpensOnMainMenu(startup::GameStart::AutoSave));
	EXPECT_FALSE(startup::OpensOnMainMenu(startup::GameStart::NewGame));
}
