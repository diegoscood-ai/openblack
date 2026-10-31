/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <array>
#include <limits>

#include <gtest/gtest.h>

#include "Input/KeyBindings.h"
#include "Input/SpeedKeys.h"

using namespace openblack::input;

TEST(SpeedKeys, SlowerHalvesTheSpeedDownToAHalf)
{
	EXPECT_FLOAT_EQ(SlowerGameSpeed(2.0f), 1.0f);
	EXPECT_FLOAT_EQ(SlowerGameSpeed(1.0f), 0.5f);
	EXPECT_FLOAT_EQ(SlowerGameSpeed(0.5f), 0.5f);
	// a speed a script set out of the keys' range: halved, or held at a half
	EXPECT_FLOAT_EQ(SlowerGameSpeed(4.0f), 2.0f);
	EXPECT_FLOAT_EQ(SlowerGameSpeed(0.1f), 0.5f);
	EXPECT_TRUE(std::isnan(SlowerGameSpeed(std::numeric_limits<float>::quiet_NaN())));
}

TEST(SpeedKeys, FasterDoublesTheSpeedUpToTwo)
{
	EXPECT_FLOAT_EQ(FasterGameSpeed(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(FasterGameSpeed(1.0f), 2.0f);
	EXPECT_FLOAT_EQ(FasterGameSpeed(2.0f), 2.0f);
	EXPECT_FLOAT_EQ(FasterGameSpeed(3.0f), 2.0f);
	EXPECT_FLOAT_EQ(FasterGameSpeed(0.1f), 0.2f);
	EXPECT_FLOAT_EQ(FasterGameSpeed(std::numeric_limits<float>::quiet_NaN()), 2.0f);
}

TEST(SpeedKeys, OnlyTheOneModifierPassedOnCounts)
{
	EXPECT_EQ(HeldModifierOf(KMOD_NONE), Modifier::None);
	EXPECT_EQ(HeldModifierOf(KMOD_LALT), Modifier::Alt);
	EXPECT_EQ(HeldModifierOf(KMOD_RALT), Modifier::Alt);
	// Shift goes before Ctrl, and Ctrl before Alt
	EXPECT_EQ(HeldModifierOf(KMOD_LALT | KMOD_LCTRL), Modifier::Ctrl);
	EXPECT_EQ(HeldModifierOf(KMOD_RALT | KMOD_RSHIFT), Modifier::Shift);
	EXPECT_EQ(HeldModifierOf(KMOD_LCTRL | KMOD_LSHIFT), Modifier::Shift);
}

TEST(SpeedKeys, AltWithOneOrTwoSetsTheSpeed)
{
	EXPECT_EQ(SpeedForKey(SDL_SCANCODE_1, KMOD_LALT, k_DefaultKeyBindings, 1.0f), 0.5f);
	EXPECT_EQ(SpeedForKey(SDL_SCANCODE_2, KMOD_RALT, k_DefaultKeyBindings, 1.0f), 2.0f);
	EXPECT_EQ(SpeedForKey(SDL_SCANCODE_1, KMOD_LALT | KMOD_NUM, k_DefaultKeyBindings, 2.0f), 1.0f);
}

TEST(SpeedKeys, OtherKeysAndModifiersSetNoSpeed)
{
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_1, KMOD_NONE, k_DefaultKeyBindings, 1.0f).has_value());
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_2, KMOD_LCTRL, k_DefaultKeyBindings, 1.0f).has_value());
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_1, KMOD_LALT | KMOD_LCTRL, k_DefaultKeyBindings, 1.0f).has_value());
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_2, KMOD_LALT | KMOD_LSHIFT, k_DefaultKeyBindings, 1.0f).has_value());
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_3, KMOD_LALT, k_DefaultKeyBindings, 1.0f).has_value());
}

TEST(SpeedKeys, AnActionBoundToTheKeyAndAltTakesIt)
{
	KeyBindingTable bindings = k_DefaultKeyBindings;
	// the key alone bound: Alt and the key still set the speed
	bindings[0].key = KeyChord {SDL_SCANCODE_1, Modifier::None};
	EXPECT_EQ(SpeedForKey(SDL_SCANCODE_1, KMOD_LALT, bindings, 1.0f), 0.5f);
	// the key with Alt bound: the action takes it
	bindings[0].key = KeyChord {SDL_SCANCODE_1, Modifier::Alt};
	EXPECT_FALSE(SpeedForKey(SDL_SCANCODE_1, KMOD_RALT, bindings, 1.0f).has_value());
	EXPECT_EQ(SpeedForKey(SDL_SCANCODE_2, KMOD_LALT, bindings, 1.0f), 2.0f);
}
