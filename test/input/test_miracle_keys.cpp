/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <algorithm>

#include <gtest/gtest.h>

#include "Input/KeyBindings.h"
#include "Input/MiracleKeys.h"

using namespace openblack::input;

TEST(MiracleKeys, RWithAnyModifierAsksForTheRepeat)
{
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_NONE, k_DefaultKeyBindings));
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_LSHIFT, k_DefaultKeyBindings));
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_RCTRL, k_DefaultKeyBindings));
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_LALT | KMOD_NUM, k_DefaultKeyBindings));
}

TEST(MiracleKeys, OtherKeysDoNot)
{
	EXPECT_FALSE(RepeatMiracleKey(SDL_SCANCODE_M, KMOD_NONE, k_DefaultKeyBindings));
	EXPECT_FALSE(RepeatMiracleKey(SDL_SCANCODE_T, KMOD_NONE, k_DefaultKeyBindings));
}

TEST(MiracleKeys, AnActionBoundToRWithTheHeldModifierTakesIt)
{
	KeyBindingTable bindings = k_DefaultKeyBindings;
	bindings[0].key = KeyChord {SDL_SCANCODE_R, Modifier::Ctrl};
	EXPECT_FALSE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_LCTRL, bindings));
	// R alone, or with another modifier, is not that binding
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_NONE, bindings));
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_LSHIFT, bindings));
	bindings[0].key = KeyChord {SDL_SCANCODE_R, Modifier::None};
	EXPECT_FALSE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_NONE, bindings));
}

TEST(MiracleKeys, MWithAnyModifierAsksForTheSelection)
{
	EXPECT_TRUE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_NONE, k_DefaultKeyBindings));
	EXPECT_TRUE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_RSHIFT, k_DefaultKeyBindings));
	EXPECT_TRUE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_LCTRL | KMOD_CAPS, k_DefaultKeyBindings));
	EXPECT_FALSE(SelectMiracleKey(SDL_SCANCODE_R, KMOD_NONE, k_DefaultKeyBindings));
	EXPECT_FALSE(SelectMiracleKey(SDL_SCANCODE_N, KMOD_NONE, k_DefaultKeyBindings));
}

TEST(MiracleKeys, AnActionBoundToMWithTheHeldModifierTakesIt)
{
	KeyBindingTable bindings = k_DefaultKeyBindings;
	bindings[0].key = KeyChord {SDL_SCANCODE_M, Modifier::Alt};
	EXPECT_FALSE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_RALT, bindings));
	EXPECT_TRUE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_NONE, bindings));
	// a binding on M does not take R
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_RALT, bindings));
}

// A key bound to an action read while it is held (the hand's buttons, the camera's moves and their modifiers) still
// reaches R and M, as does any bound key under a script's cinema bars, where no action acts on its key
TEST(MiracleKeys, AKeyBoundToAHeldActionOrUnderTheCinemaBarsGoesOnToRAndM)
{
	KeyBindingTable bindings = k_DefaultKeyBindings;
	bindings[0].key = KeyChord {SDL_SCANCODE_R, Modifier::None};
	// the help acts on its key: R is taken, but not under the bars
	EXPECT_FALSE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_NONE, bindings, false));
	EXPECT_TRUE(RepeatMiracleKey(SDL_SCANCODE_R, KMOD_NONE, bindings, true));
	// Move Left is read while held
	bindings = k_DefaultKeyBindings;
	const auto moveLeft = std::ranges::find(bindings, BindableActionMap::MOVE_LEFT, &KeyBinding::action);
	ASSERT_NE(moveLeft, bindings.end());
	moveLeft->key = KeyChord {SDL_SCANCODE_M, Modifier::None};
	EXPECT_TRUE(SelectMiracleKey(SDL_SCANCODE_M, KMOD_NONE, bindings));
}

// The actions that act on their key going down, and those read while held
TEST(MiracleKeys, TheActionsThatTakeTheirKeyAreThoseThatActOnItGoingDown)
{
	for (const auto action :
	     {BindableActionMap::HELP, BindableActionMap::ZOOM_TO_TEMPLE, BindableActionMap::ZOOM_TO_CREATURE,
	      BindableActionMap::ZOOM_TO_REALM, BindableActionMap::ZOOM_TO_INSIDE_TEMPLE, BindableActionMap::ZOOM_TO_LIBRARY,
	      BindableActionMap::LEASH_UNLEASH_CREATURE, BindableActionMap::TALK, BindableActionMap::SHOW_VILLAGER_NAMES,
	      BindableActionMap::SHOW_VILLAGER_DETAILS, BindableActionMap::QUICK_SAVE, BindableActionMap::QUICK_LOAD,
	      BindableActionMap::NEXT_LEASH, BindableActionMap::PREVIOUS_LEASH})
	{
		EXPECT_TRUE(ActsOnKeyDown(action)) << static_cast<uint64_t>(action);
	}
	for (const auto action : {BindableActionMap::MOVE, BindableActionMap::ACTION, BindableActionMap::ZOOM_OUT,
	                          BindableActionMap::ZOOM_IN, BindableActionMap::ZOOM_ON, BindableActionMap::MOVE_LEFT,
	                          BindableActionMap::MOVE_BACKWARDS, BindableActionMap::TILT_UP, BindableActionMap::ROTATE_RIGHT,
	                          BindableActionMap::ROTATE_ON, BindableActionMap::ROTATE_AROUND_MOUSE_ON})
	{
		EXPECT_FALSE(ActsOnKeyDown(action)) << static_cast<uint64_t>(action);
	}
}

// The bookmark, Tab and Backspace keys ask the bindings too: a key whose action takes it goes no further
TEST(MiracleKeys, ABoundKeyThatActsIsTakenBeforeTheBookmarkAndJumpKeys)
{
	KeyBindingTable bindings = k_DefaultKeyBindings;
	EXPECT_FALSE(KeyTakenByBinding(SDL_SCANCODE_1, KMOD_LCTRL, bindings, false));
	bindings[0].key = KeyChord {SDL_SCANCODE_1, Modifier::Ctrl};
	EXPECT_TRUE(KeyTakenByBinding(SDL_SCANCODE_1, KMOD_LCTRL, bindings, false));
	EXPECT_FALSE(KeyTakenByBinding(SDL_SCANCODE_1, KMOD_NONE, bindings, false));
	EXPECT_FALSE(KeyTakenByBinding(SDL_SCANCODE_1, KMOD_LCTRL, bindings, true));
	bindings[0].key = KeyChord {SDL_SCANCODE_TAB, Modifier::None};
	EXPECT_TRUE(KeyTakenByBinding(SDL_SCANCODE_TAB, KMOD_NONE, bindings, false));
}
