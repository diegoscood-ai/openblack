/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <gtest/gtest.h>

#include "Graphics/AtmosGlow.h"

using namespace openblack::graphics;

// The atmosphere's screen glow's rules (docs/wiki/world/weather.md#the-screen-glow). The quad's colour and when it is
// drawn are the original's end of frame call run in the oracle; the grab locks the screen and the texture, so its
// steps are pinned to its integer code instead

TEST(AtmosGlow, CellsReadThePixelsInTheMiddleOfTheirSixteenths)
{
	EXPECT_EQ(atmos_glow::CellPixel(0, 0, {640, 480}), glm::ivec2(20, 15));
	EXPECT_EQ(atmos_glow::CellPixel(15, 15, {640, 480}), glm::ivec2(620, 465));
	// Every division a whole one: 1366 / 16 = 85, 85 / 2 = 42; 7 * 768 / 16 = 336, 768 / 32 = 24
	EXPECT_EQ(atmos_glow::CellPixel(1, 7, {1366, 768}), glm::ivec2(127, 360));
	// The rows from the whole height times the row, then divided: 3 * 601 / 16 = 112, 601 / 32 = 18
	EXPECT_EQ(atmos_glow::CellPixel(0, 3, {800, 601}), glm::ivec2(25, 130));
}

TEST(AtmosGlow, ACellMovesAnEighthOfTheWayToTheScreen)
{
	// 128 times 4096 is 0x80000: an eighth of it, then an eighth of what is left
	int32_t value = 0;
	value = atmos_glow::StepChannel(value, 128);
	EXPECT_EQ(value, 0x10000);
	value = atmos_glow::StepChannel(value, 128);
	EXPECT_EQ(value, 0x1E000);
	value = atmos_glow::StepChannel(value, 128);
	EXPECT_EQ(value, 0x2A400);
	// It stops 7 short, where an eighth of what is left is cut to 0
	for (int frame = 3; frame < 100; ++frame)
	{
		value = atmos_glow::StepChannel(value, 128);
	}
	EXPECT_EQ(value, 0x7FFF9);
	EXPECT_EQ(atmos_glow::StepChannel(value, 128), 0x7FFF9);
}

TEST(AtmosGlow, TheEighthIsCutTowardsZero)
{
	// Down to black: 0x80000 - 0x10000, then never below 7, as -7 / 8 is 0
	EXPECT_EQ(atmos_glow::StepChannel(0x80000, 0), 0x70000);
	EXPECT_EQ(atmos_glow::StepChannel(15, 0), 14);
	EXPECT_EQ(atmos_glow::StepChannel(7, 0), 7);
	EXPECT_EQ(atmos_glow::StepChannel(-9, 0), 0);
}

TEST(AtmosGlow, ACellIsHeldBelowTheWhiteOfTheScreen)
{
	// White, 0xFF000, is held to 0xF7000 from the 26th frame
	int32_t value = 0;
	for (int frame = 0; frame < 25; ++frame)
	{
		value = atmos_glow::StepChannel(value, 255);
	}
	EXPECT_EQ(value, 0xF5F29);
	EXPECT_EQ(atmos_glow::StepChannel(value, 255), atmos_glow::k_CellLimit);
	EXPECT_EQ(atmos_glow::StepChannel(atmos_glow::k_CellLimit, 255), atmos_glow::k_CellLimit);
	EXPECT_EQ(atmos_glow::StepChannel(0xF6FFF, 255), atmos_glow::k_CellLimit);
}

TEST(AtmosGlow, TheTexelRoundsEachChannelItsOwnWay)
{
	// Alpha 15 always; red rounded up from 0x80, green from 0x800, blue from 0x8000 below the next 16th
	EXPECT_EQ(atmos_glow::PackCell(0, 0, 0), 0xF000);
	EXPECT_EQ(atmos_glow::PackCell(0xFF7F, 0xF7FF, 0x7FFF), 0xF000);
	EXPECT_EQ(atmos_glow::PackCell(0xFF80, 0xF800, 0x8000), 0xF111);
	EXPECT_EQ(atmos_glow::PackCell(0x7FFF9, 0x7FFF9, 0x7FFF9), 0xF888);
	EXPECT_EQ(atmos_glow::PackCell(atmos_glow::k_CellLimit, atmos_glow::k_CellLimit, atmos_glow::k_CellLimit), 0xFFFF);
	EXPECT_EQ(atmos_glow::PackCell(0x30000, 0x50000, 0xA0000), 0xF35A);
}

TEST(AtmosGlow, TheQuadIsGreyByTheDarkness)
{
	// The colours the original's end of frame passes to the quad's draw, from the oracle
	EXPECT_EQ(atmos_glow::QuadColour(1), 0xFF010101u);
	EXPECT_EQ(atmos_glow::QuadColour(47), 0xFF2F2F2Fu);
	EXPECT_EQ(atmos_glow::QuadColour(89), 0xFF595959u);
	EXPECT_EQ(atmos_glow::QuadColour(90), 0xFF5A5A5Au);
}

TEST(AtmosGlow, OnlyTheWorldsViewWithADarkSkyHasIt)
{
	const atmos_glow::FrameInputs world {.weather = true, .darkness = 40, .worldDrawn = true};
	EXPECT_EQ(atmos_glow::ForFrame(world), (atmos_glow::FrameUse {.steps = true, .draws = true}));
	// No darkness or no Weather setting: none (the oracle's end of frame draws nothing either way)
	auto clear = world;
	clear.darkness = 0;
	EXPECT_EQ(atmos_glow::ForFrame(clear), atmos_glow::FrameUse {});
	auto off = world;
	off.weather = false;
	EXPECT_EQ(atmos_glow::ForFrame(off), atmos_glow::FrameUse {});
	// Under a film that covers the screen the land is not drawn, so the glow is not added, but the frame still steps it
	auto film = world;
	film.worldDrawn = false;
	EXPECT_EQ(atmos_glow::ForFrame(film), (atmos_glow::FrameUse {.steps = true, .draws = false}));
	// Neither in the temple nor in the falling spell's film
	auto temple = film;
	temple.temple = true;
	EXPECT_EQ(atmos_glow::ForFrame(temple), atmos_glow::FrameUse {});
	auto falling = film;
	falling.fallingSpell = true;
	EXPECT_EQ(atmos_glow::ForFrame(falling), atmos_glow::FrameUse {});
}
