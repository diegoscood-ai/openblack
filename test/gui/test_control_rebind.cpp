/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <optional>

#include <SDL_scancode.h>
#include <gtest/gtest.h>

#include "Gui/ControlBindings.h"
#include "Gui/ControlRebind.h"
#include "Input/DirectInputKeys.h"
#include "Input/KeyBindings.h"

using namespace openblack;
using namespace openblack::gui::control_rebind;
using openblack::gui::control_bindings::k_Bindings;
using openblack::gui::control_bindings::Mouse;

TEST(ControlRebind, EscapeTabPF1F2AndTheKeypadsEnterAreNeverBound)
{
	for (const uint8_t key : std::array<uint8_t, 6> {0x01, 0x0F, 0x19, 0x3B, 0x3C, 0x9C})
	{
		EXPECT_FALSE(IsBindableKey(key)) << static_cast<int>(key);
	}
	// Return, F3, O, the left Shift and the space are
	for (const uint8_t key : std::array<uint8_t, 5> {0x1C, 0x3D, 0x18, 0x2A, 0x39})
	{
		EXPECT_TRUE(IsBindableKey(key)) << static_cast<int>(key);
	}
}

TEST(ControlRebind, AKeyTakesOneModifierShiftFirstThenCtrlThenAlt)
{
	constexpr uint8_t k_S = 0x1F;
	EXPECT_EQ(ModifierWith(k_S, false, false, false), 0);
	EXPECT_EQ(ModifierWith(k_S, true, false, false), k_Shift);
	EXPECT_EQ(ModifierWith(k_S, false, true, false), k_Ctrl);
	EXPECT_EQ(ModifierWith(k_S, false, false, true), k_Alt);
	// Shift wins over the others
	EXPECT_EQ(ModifierWith(k_S, true, true, true), k_Shift);
	// a modifier key, of either side, is not its own modifier
	EXPECT_EQ(ModifierWith(0x36, true, false, false), 0);
	EXPECT_EQ(ModifierWith(0x9D, false, true, false), 0);
	EXPECT_EQ(ModifierWith(0xB8, false, false, true), 0);
	// a Shift key with Ctrl held is in the Shift branch: no modifier; Ctrl held with an Alt key: Ctrl
	EXPECT_EQ(ModifierWith(0x2A, true, true, false), 0);
	EXPECT_EQ(ModifierWith(0x38, false, true, true), k_Ctrl);
}

TEST(ControlRebind, AKeyBoundIsTakenOffEveryOtherActionWithTheSameKeyAndModifier)
{
	auto bindings = k_Bindings;
	// Tilt Up (A) gets Talk's T; Talk loses it
	AssignKey(bindings, 11, 0x14, 0);
	EXPECT_EQ(bindings[11].key, 0x14);
	EXPECT_EQ(bindings[5].key, 0);
	// S alone is not Ctrl+S: the quick save keeps it when Help takes S
	bindings = k_Bindings;
	AssignKey(bindings, 0, 0x1F, 0);
	EXPECT_EQ(bindings[28].key, 0);
	EXPECT_EQ(bindings[29].key, 0x1F);
	EXPECT_EQ(bindings[29].modifier, k_Ctrl);
	// no key takes the action's away and leaves the others
	bindings = k_Bindings;
	AssignKey(bindings, 5, 0, 0);
	EXPECT_EQ(bindings[5].key, 0);
	EXPECT_EQ(bindings[11].key, 0x1E);
	// the buttons are left alone
	EXPECT_EQ(bindings[1].mouse, Mouse::Left);
}

TEST(ControlRebind, AMouseButtonBoundIsTakenOffEveryActionFirst)
{
	auto bindings = k_Bindings;
	// Help takes the left button; Move loses it, Help keeps its F1
	AssignMouse(bindings, 0, Mouse::Left);
	EXPECT_EQ(bindings[0].mouse, Mouse::Left);
	EXPECT_EQ(bindings[1].mouse, Mouse::None);
	EXPECT_EQ(bindings[0].key, 0x3B);
	// none takes the action's button
	AssignMouse(bindings, 2, Mouse::None);
	EXPECT_EQ(bindings[2].mouse, Mouse::None);
	EXPECT_EQ(bindings[0].mouse, Mouse::Left);
}

TEST(ControlRebind, ThePagesDefaultsAreTheActionMapsDefaults)
{
	// every row of the page, turned into the action map's codes, is the action map's default binding
	ASSERT_EQ(k_Bindings.size(), input::k_DefaultKeyBindings.size());
	for (size_t i = 0; i < k_Bindings.size(); ++i)
	{
		EXPECT_EQ(ChordOf(k_Bindings[i]), input::k_DefaultKeyBindings[i].key) << i;
		EXPECT_EQ(MouseInputOf(k_Bindings[i].mouse), input::k_DefaultKeyBindings[i].mouse) << i;
	}
}

// An action's picture on the Controls page: its own button; with none, the zoom out, the zoom in and the zoom on show
// both buttons while the first two actions have the left and the right button, either way round
TEST(ControlRebind, TheZoomActionsWithNoButtonShowBothButtonsWhileTheFirstTwoAreTheButtons)
{
	using openblack::gui::control_bindings::Mouse;
	using openblack::gui::control_bindings::PictureOf;
	auto bindings = openblack::gui::control_bindings::k_Bindings;
	EXPECT_EQ(bindings.at(6).mouse, Mouse::None);
	EXPECT_EQ(PictureOf(bindings, 6), Mouse::Both);
	EXPECT_EQ(PictureOf(bindings, 3), Mouse::WheelDown);
	EXPECT_EQ(PictureOf(bindings, 1), Mouse::Left);
	EXPECT_EQ(PictureOf(bindings, 0), Mouse::None);
	bindings.at(3).mouse = Mouse::None;
	EXPECT_EQ(PictureOf(bindings, 3), Mouse::Both);
	// the two buttons the other way round
	bindings.at(1).mouse = Mouse::Right;
	bindings.at(2).mouse = Mouse::Left;
	EXPECT_EQ(PictureOf(bindings, 6), Mouse::Both);
	// another action with no button shows none
	EXPECT_EQ(PictureOf(bindings, 5), Mouse::None);
	// not the two buttons: none
	bindings.at(1).mouse = Mouse::Middle;
	EXPECT_EQ(PictureOf(bindings, 6), Mouse::None);
	EXPECT_EQ(PictureOf(bindings, 3), Mouse::None);
	EXPECT_EQ(PictureOf(bindings, 99), Mouse::None);
}

TEST(ControlRebind, TheGamesKeyCodesAreSdlsKeysBothWays)
{
	EXPECT_EQ(input::direct_input::ScancodeOf(0x1E), SDL_SCANCODE_A);
	EXPECT_EQ(input::direct_input::ScancodeOf(0x9D), SDL_SCANCODE_RCTRL);
	EXPECT_EQ(input::direct_input::ScancodeOf(0xC8), SDL_SCANCODE_UP);
	EXPECT_EQ(input::direct_input::ScancodeOf(0x54), std::nullopt);
	EXPECT_EQ(input::direct_input::CodeOf(SDL_SCANCODE_KP_ENTER), 0x9C);
	EXPECT_EQ(input::direct_input::CodeOf(SDL_SCANCODE_F13), std::nullopt);
	for (int code = 1; code < 256; ++code)
	{
		if (const auto key = input::direct_input::ScancodeOf(static_cast<uint8_t>(code)))
		{
			EXPECT_EQ(input::direct_input::CodeOf(*key), code) << code;
		}
	}
}
