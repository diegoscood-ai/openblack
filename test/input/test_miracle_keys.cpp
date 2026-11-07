/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

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
