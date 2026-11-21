/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <array>
#include <string>

#include <gtest/gtest.h>

#include "Help/ClickCue.h"
#include "Help/ControlIcon.h"
#include "Input/KeyBindings.h"

using namespace openblack::help;

TEST(ControlIcon, TheActionsFromSixToFifteenAreKeys)
{
	EXPECT_FALSE(control_icon::IsKeyAction(5));
	EXPECT_TRUE(control_icon::IsKeyAction(6));
	EXPECT_TRUE(control_icon::IsKeyAction(15));
	EXPECT_FALSE(control_icon::IsKeyAction(16));
	EXPECT_FALSE(control_icon::IsKeyAction(-1));
}

TEST(ControlIcon, TheSecondKeyFollowsAPlusAndTheOthersASlash)
{
	std::u16string text;
	int count = 0;
	control_icon::AppendKeyName(text, count, u"Shift");
	EXPECT_EQ(text, u"Shift");
	control_icon::AppendKeyName(text, count, u"A");
	EXPECT_EQ(text, u"Shift + A");
	control_icon::AppendKeyName(text, count, u"B");
	EXPECT_EQ(text, u"Shift + A / B");
	EXPECT_EQ(count, 3);
}

TEST(ControlIcon, AKeySitsInTheBoxsTopThirdAndAMouseOnTheCuesRow)
{
	// A box from 400 to 459: the icon is (459 - 400 + 1) / 3 = 20 high
	EXPECT_EQ(click_cue::CueSize(400, 459), 20);
	EXPECT_EQ(control_icon::KeyIconRow(400, 459), 410);
	EXPECT_EQ(click_cue::MiddleRow(400, 459), 450);
	// Cut towards zero: 62 high gives 20, and half of it 10
	EXPECT_EQ(control_icon::KeyIconRow(0, 61), 10);
}

TEST(ControlIcon, AKeyActionNamesItsPrimaryKeyAloneWhateverButtonIsBoundBesideIt)
{
	using openblack::input::k_DefaultKeyBindings;
	using openblack::input::KeyChord;
	using openblack::input::Modifier;
	using openblack::input::MouseInput;
	// the defaults: Zoom On is the right Ctrl key, Tilt/Rotate On the right Shift key, Move Left the left arrow
	EXPECT_EQ(control_icon::PrimaryKey(k_DefaultKeyBindings, 6), SDL_SCANCODE_RCTRL);
	EXPECT_EQ(control_icon::PrimaryKey(k_DefaultKeyBindings, 7), SDL_SCANCODE_LEFT);
	EXPECT_EQ(control_icon::PrimaryKey(k_DefaultKeyBindings, 15), SDL_SCANCODE_RSHIFT);
	auto bindings = k_DefaultKeyBindings;
	// a mouse button bound beside the key, and a modifier with it: still the key, alone
	bindings.at(6).mouse = MouseInput::MiddleButton;
	bindings.at(6).key = KeyChord {.key = SDL_SCANCODE_X, .modifier = Modifier::Shift};
	EXPECT_EQ(control_icon::PrimaryKey(bindings, 6), SDL_SCANCODE_X);
	// a mouse button and no key: nothing
	bindings.at(15).key.reset();
	bindings.at(15).mouse = MouseInput::RightButton;
	EXPECT_EQ(control_icon::PrimaryKey(bindings, 15), std::nullopt);
	// an action the bindings do not hold: nothing
	EXPECT_EQ(control_icon::PrimaryKey(bindings, -1), std::nullopt);
	EXPECT_EQ(control_icon::PrimaryKey(bindings, static_cast<int>(bindings.size())), std::nullopt);
}
