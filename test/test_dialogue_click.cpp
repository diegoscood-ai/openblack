/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Which mouse button event is the dialogue's click, and what a text's cut leaves of the press it takes from the hand

#include <gtest/gtest.h>

#include "ECS/HandPressChain.h"
#include "Help/DialogueClick.h"
#include "Input/MouseButtons.h"

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

namespace
{
// The hand's reading of the Action button at each pass, as the hand system keeps it: a new press, a release, and the
// hold it carries to the next pass
struct FakeHandPass
{
	bool heldAtLastPass {false};
	int presses {0};
	int releases {0};

	void Pass(const input::MouseButtonsState& buttons)
	{
		if (ecs::hand_press::IsNewPress(buttons.right, heldAtLastPass))
		{
			++presses;
		}
		if (!buttons.right && heldAtLastPass)
		{
			++releases;
		}
		heldAtLastPass = buttons.right;
	}

	void Cut(ecs::hand_press::InterfaceCut cut)
	{
		if (ecs::hand_press::PressOverAtCut(cut))
		{
			heldAtLastPass = false;
		}
	}
};

input::MouseButtonEvent Action(bool down)
{
	return {.button = MouseButton::Right, .down = down};
}
} // namespace

TEST(DialogueClick, TheActionButtonHeldThroughATextsCutIsNoNewPress)
{
	// the Action button pressed on a ball, the keypad's Enter cutting a text while it is held, then let go: the cut
	// ends the press's action, and the button still held does not press the ball again (whose release would tap it)
	input::MouseButtonsState buttons;
	FakeHandPass hand;
	EXPECT_TRUE(input::ApplyMouseButton(buttons, Action(true)));
	hand.Pass(buttons);
	EXPECT_EQ(hand.presses, 1);
	hand.Cut(ecs::hand_press::InterfaceCut::Text);
	for (int pass = 0; pass < 4; ++pass)
	{
		hand.Pass(buttons);
	}
	EXPECT_EQ(hand.presses, 1);
	EXPECT_EQ(hand.releases, 0);
	// its release still comes, to a hand whose action already ended
	EXPECT_TRUE(input::ApplyMouseButton(buttons, Action(false)));
	hand.Pass(buttons);
	EXPECT_EQ(hand.presses, 1);
	EXPECT_EQ(hand.releases, 1);
	// the next press is an ordinary one
	EXPECT_TRUE(input::ApplyMouseButton(buttons, Action(true)));
	hand.Pass(buttons);
	EXPECT_EQ(hand.presses, 2);
}

TEST(DialogueClick, ABoxsFrameClearsTheHoldTheTextsCutLeaves)
{
	// a box's frame clears the buttons' hold with the action: the hand sees neither a release nor a new press
	input::MouseButtonsState buttons;
	FakeHandPass hand;
	(void)input::ApplyMouseButton(buttons, Action(true));
	hand.Pass(buttons);
	(void)input::BoxTakesInput(buttons, true);
	hand.Cut(ecs::hand_press::InterfaceCut::Box);
	hand.Pass(buttons);
	EXPECT_EQ(hand.presses, 1);
	EXPECT_EQ(hand.releases, 0);
	EXPECT_TRUE(ecs::hand_press::PressOverAtCut(ecs::hand_press::InterfaceCut::Box));
	EXPECT_FALSE(ecs::hand_press::PressOverAtCut(ecs::hand_press::InterfaceCut::Text));
}
