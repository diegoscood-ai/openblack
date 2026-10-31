/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The scripts' words on the screen as the cut scenes' director keeps them: the newest first, in the colour set when
// each was written, faded together, and gone at once at the scripts' restart, which keeps the colour.
// docs/wiki/interface/screen-and-readouts.md (the scripts' screen texts)

#define LOCATOR_IMPLEMENTATIONS

#include <gtest/gtest.h>

#include "ECS/Systems/Implementations/CinematicDirectorSystem.h"

using openblack::ecs::systems::CinematicDirectorSystem;

TEST(CinematicDirectorTexts, TheNewestIsFirstInTheColourSetWhenItWasWritten)
{
	CinematicDirectorSystem director;
	director.WriteText(u"Peter", {0.1f, 0.3f}, {0.4f, 0.1f}, 0.05f, 2.0f);
	director.SetTextColour({255, 255, 255});
	director.WriteText(u"Mark", {0.1f, 0.35f}, {0.4f, 0.1f}, 0.05f, 0.0f);
	const auto texts = director.GetTexts();
	ASSERT_EQ(texts.size(), 2u);
	EXPECT_EQ(texts[0].words, u"Mark");
	EXPECT_EQ(texts[0].colour, glm::u8vec3(255, 255, 255));
	EXPECT_EQ(texts[1].words, u"Peter");
	// black until a script sets a colour
	EXPECT_EQ(texts[1].colour, glm::u8vec3(0, 0, 0));
}

TEST(CinematicDirectorTexts, AllFadeOutTogetherAndGoOnceFaded)
{
	CinematicDirectorSystem director;
	director.WriteText(u"one", {}, {0.5f, 0.1f}, 0.05f, 0.0f);
	director.WriteText(u"two", {}, {0.5f, 0.1f}, 0.05f, 0.0f);
	director.FadeAllTexts(3.0f);
	director.UpdateTexts(1500);
	ASSERT_EQ(director.GetTexts().size(), 2u);
	EXPECT_NEAR(director.GetTexts()[0].alpha, 0.5f, 1e-5f);
	director.UpdateTexts(1500);
	EXPECT_TRUE(director.GetTexts().empty());
}

TEST(CinematicDirectorTexts, TheScriptsRestartTakesThemAwayAndKeepsTheColour)
{
	CinematicDirectorSystem director;
	director.SetTextColour({128, 128, 128});
	director.WriteText(u"Game Over", {0.25f, 0.45f}, {0.5f, 0.1f}, 0.075f, 3.0f);
	director.OnScriptsRestart();
	director.UpdateTexts(0);
	EXPECT_TRUE(director.GetTexts().empty());
	director.WriteText(u"again", {}, {0.5f, 0.1f}, 0.05f, 0.0f);
	EXPECT_EQ(director.GetTexts()[0].colour, glm::u8vec3(128, 128, 128));
}
