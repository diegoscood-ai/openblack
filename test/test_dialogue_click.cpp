/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which mouse button event is the dialogue's click

#include <gtest/gtest.h>

#include "Help/DialogueClick.h"

using namespace openblack;
using help::dialogue_click::IsClick;
using input::MouseButton;

TEST(DialogueClick, TheLeftButtonsReleaseIsTheClickNotItsPress)
{
	EXPECT_TRUE(IsClick(MouseButton::Left, false));
	EXPECT_FALSE(IsClick(MouseButton::Left, true));
}

TEST(DialogueClick, NoOtherButtonClicks)
{
	for (const bool down : {true, false})
	{
		EXPECT_FALSE(IsClick(MouseButton::Right, down));
		EXPECT_FALSE(IsClick(MouseButton::Middle, down));
	}
}

TEST(DialogueClick, OnlyTheAnswerThatTheTextTookTheClickTakesItFromTheHand)
{
	// the text cut short (a click in a cinema, the key anywhere): the hand's action ends for the pass
	EXPECT_TRUE(help::dialogue_click::TakesClickFromHand(help::dialogue_click::k_ClickTaken));
	EXPECT_EQ(help::dialogue_click::k_ClickTaken, 0x14);
	// the click ignored, or only ending a text's wait for it outside a cinema: it goes on to the hand
	EXPECT_FALSE(help::dialogue_click::TakesClickFromHand(1));
	EXPECT_FALSE(help::dialogue_click::TakesClickFromHand(0));
}
