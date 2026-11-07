/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// raffclar's cases (0222f9f5, gui/test_screen_fade.cpp) on our gui::ScreenFade: his seconds are our milliseconds, his
// colour our 0xAARRGGBB with the alpha byte the fade truncated to 0..255. Where his cases count a turn when the fade
// lands exactly on its target, these hold the original's: it counts only past the target, and a fade that lands on it
// is held there (docs/wiki/video/playback.md#the-falling-spell-fallbik).

#include <cstdint>

#include <gtest/gtest.h>

#include "GameClock.h"
#include "Gui/ScreenFade.h"

using namespace openblack;
using namespace openblack::gui;

namespace
{
constexpr uint32_t k_White = 0xFFFFFF;

/// The fade's colour as it stands: a step of no time changes nothing
uint32_t Colour(ScreenFade& fade)
{
	return fade.Update(0);
}

/// `frames` steps of `deltaMs`, the colour of the last
uint32_t Step(ScreenFade& fade, uint32_t deltaMs, int frames)
{
	uint32_t colour = 0;
	for (int i = 0; i < frames; ++i)
	{
		colour = fade.Update(deltaMs);
	}
	return colour;
}

[[nodiscard]] constexpr uint32_t Alpha(uint32_t colour)
{
	return colour >> 24;
}

[[nodiscard]] constexpr uint32_t Rgb(uint32_t colour)
{
	return colour & 0x00FFFFFFu;
}
} // namespace

TEST(ScreenFade, StartsClear)
{
	ScreenFade fade;
	EXPECT_EQ(Alpha(Colour(fade)), 0u);
}

TEST(ScreenFade, FadesAwayOverASecond)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f, k_White);
	EXPECT_EQ(Colour(fade), 0xFFFFFFFFu);
	// 250 ms is 0.25 exactly: half way, 0.5 x 255 truncated
	const uint32_t half = Step(fade, 250, 2);
	EXPECT_EQ(Alpha(half), 127u);
	EXPECT_EQ(Rgb(half), k_White);
	// It lands exactly on 0, not past it: clear, but held there with no turn and its colour kept
	EXPECT_EQ(Step(fade, 250, 2), k_White);
	EXPECT_EQ(fade.current, 0.0f);
	EXPECT_EQ(fade.GetTurns(), 0);
	EXPECT_FALSE(fade.Runs(game_clock::k_SequenceModeNone));
}

TEST(ScreenFade, StaysCoveredBeyondAWhole)
{
	ScreenFade fade;
	fade.FadeFrom(1.2f);
	EXPECT_EQ(Alpha(Step(fade, 100, 1)), 0xFFu);
	// About 0.9: 229.5 truncated
	EXPECT_EQ(Alpha(Step(fade, 100, 2)), 229u);
	EXPECT_NEAR(fade.current, 0.9f, 1e-5f);
}

TEST(ScreenFade, KeepsItsColourUntilFadedAway)
{
	ScreenFade fade;
	fade.FadeFrom(1.0f, k_White);
	Step(fade, 500, 1);
	// A fade without a colour of its own takes on the one still fading
	fade.FadeFrom(1.2f);
	EXPECT_EQ(Rgb(Colour(fade)), k_White);
	Step(fade, 500, 3);
	// Faded away, the colour goes back to black for the next
	fade.FadeFrom(1.0f);
	EXPECT_EQ(Colour(fade), 0xFF000000u);
}

TEST(ScreenFade, FadesThroughAColour)
{
	ScreenFade fade;
	fade.FadeThrough(k_White);
	EXPECT_EQ(Alpha(Step(fade, 500, 1)), 127u);
	EXPECT_EQ(fade.GetTurns(), 0);
	// It lands exactly on the whole colour, not past it: no turn, and it is held there rather than fading back
	EXPECT_EQ(Step(fade, 500, 1), 0xFFFFFFFFu);
	EXPECT_EQ(fade.GetTurns(), 0);
	EXPECT_EQ(fade.target, 1.0f);
	EXPECT_FALSE(fade.Runs(game_clock::k_SequenceModeNone));
}
