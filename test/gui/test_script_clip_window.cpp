/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' clipping window: the screen black but for a box, the black fading in and out by the game's time, drawn
// as four boxes about the window. docs/wiki/interface/screen-and-readouts.md#the-scripts-clipping-window

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"
#include "Gui/ScriptTexts.h"

using namespace openblack::gui::script_text;

TEST(ScriptClipWindow, TheBlackFadesInOverItsTimeAndOutAgain)
{
	ClipWindow clip;
	// Land 5's credits: across 0.1, down 0.2, width 0.4, height 0.6, in half a second
	OpenClip(clip, {0.1f, 0.2f}, {0.4f, 0.6f}, 0.5f);
	EXPECT_FLOAT_EQ(clip.alpha, 0.0f);
	EXPECT_FALSE(ClipAlphaByte(clip).has_value());
	StepClip(clip, 250);
	EXPECT_FLOAT_EQ(clip.alpha, 0.5f);
	StepClip(clip, 500);
	EXPECT_FLOAT_EQ(clip.alpha, 1.0f);
	// the black's own 255 times 255, a byte down
	EXPECT_EQ(ClipAlphaByte(clip).value_or(0), 254);
	CloseClip(clip, 0.5f);
	StepClip(clip, 250);
	EXPECT_FLOAT_EQ(clip.alpha, 0.5f);
	EXPECT_EQ(ClipAlphaByte(clip).value_or(0), (255 * 127) >> 8);
	StepClip(clip, 250);
	EXPECT_FLOAT_EQ(clip.alpha, 0.0f);
	EXPECT_FALSE(ClipAlphaByte(clip).has_value());
	// the box stays where it was
	EXPECT_EQ(clip.corner, glm::vec2(0.1f, 0.2f));
}

TEST(ScriptClipWindow, WithoutATimeItIsAtOnce)
{
	ClipWindow clip;
	OpenClip(clip, {0.5f, 0.2f}, {0.4f, 0.6f}, 0.0f);
	EXPECT_FLOAT_EQ(clip.alpha, 1.0f);
	EXPECT_FLOAT_EQ(clip.rate, 0.0f);
	CloseClip(clip, 0.0f);
	EXPECT_FLOAT_EQ(clip.alpha, 0.0f);
	EXPECT_FLOAT_EQ(clip.target, 0.0f);
}

TEST(ScriptClipWindow, TheFourBoxesCoverAllButTheWindow)
{
	ClipWindow clip;
	OpenClip(clip, {0.1f, 0.2f}, {0.4f, 0.6f}, 0.0f);
	// on 800 by 600 the window's edges are 80, 120, 400 and 480, and it shows the pixels 81..400 across and 121..480
	// down
	const auto boxes = ClipBoxes(clip, {800, 600});
	EXPECT_EQ(boxes[0].min, glm::ivec2(0, 0));
	EXPECT_EQ(boxes[0].max, glm::ivec2(801, 121));
	EXPECT_EQ(boxes[1].min, glm::ivec2(0, 481));
	EXPECT_EQ(boxes[1].max, glm::ivec2(801, 601));
	EXPECT_EQ(boxes[2].min, glm::ivec2(0, 121));
	EXPECT_EQ(boxes[2].max, glm::ivec2(81, 481));
	EXPECT_EQ(boxes[3].min, glm::ivec2(401, 121));
	EXPECT_EQ(boxes[3].max, glm::ivec2(801, 481));
}

TEST(ScriptClipWindow, TheDirectorStepsItWithTheTextsAndTheScriptsRestartTakesItAway)
{
	openblack::ecs::systems::CinematicDirectorSystem director;
	director.SetClipWindow({0.1f, 0.2f}, {0.4f, 0.6f}, 0.5f);
	director.UpdateTexts(250);
	EXPECT_FLOAT_EQ(director.GetClipWindow().alpha, 0.5f);
	director.OnScriptsRestart();
	EXPECT_FLOAT_EQ(director.GetClipWindow().alpha, 0.0f);
	EXPECT_FALSE(ClipAlphaByte(director.GetClipWindow()).has_value());
}
