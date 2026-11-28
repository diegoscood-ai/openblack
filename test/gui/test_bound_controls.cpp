/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <map>
#include <span>

#include <gtest/gtest.h>

#include "Common/PlayerProfiles.h"
#include "Gui/BoundControls.h"
#include "Gui/ControlBindings.h"
#include "Input/DirectInputKeys.h"
#include "Input/KeyBindings.h"

using namespace openblack;
using gui::control_bindings::Binding;
using gui::control_bindings::k_Bindings;
using gui::control_bindings::Mouse;

namespace
{
void ExpectSame(std::span<const Binding> bindings, std::span<const Binding> expected)
{
	ASSERT_EQ(bindings.size(), expected.size());
	for (size_t i = 0; i < bindings.size(); ++i)
	{
		EXPECT_EQ(bindings[i].name, expected[i].name) << i;
		EXPECT_EQ(bindings[i].key, expected[i].key) << i;
		EXPECT_EQ(bindings[i].modifier, expected[i].modifier) << i;
		EXPECT_EQ(bindings[i].mouse, expected[i].mouse) << i;
	}
}
} // namespace

// Every key the defaults bind has its game code and back
TEST(BoundControls, EveryDefaultKeyHasItsGameCode)
{
	for (const auto& binding : input::k_DefaultKeyBindings)
	{
		if (!binding.key.has_value())
		{
			continue;
		}
		const auto code = input::direct_input::CodeOf(binding.key->key);
		ASSERT_TRUE(code.has_value()) << binding.name;
		EXPECT_EQ(input::direct_input::ScancodeOf(*code), binding.key->key) << binding.name;
	}
}

// The action map's defaults are the page's defaults
TEST(BoundControls, TheDefaultsAreThePagesDefaults)
{
	ExpectSame(gui::bound_controls::Current(input::k_DefaultKeyBindings), k_Bindings);
}

// A key bound anew is named in the game's codes, its modifier as the left key; a button taken away is none
TEST(BoundControls, TheBindingsBoundAnewAreThePagesRows)
{
	auto bound = input::k_DefaultKeyBindings;
	bound.at(13).key = input::KeyChord {SDL_SCANCODE_X, input::Modifier::Ctrl};
	bound.at(0).key.reset();
	auto expected = k_Bindings;
	expected.at(13).key = 0x2D;
	expected.at(13).modifier = 0x1D;
	expected.at(0).key = 0;
	ExpectSame(gui::bound_controls::Current(bound), expected);

	bound.at(1).mouse = input::MouseInput::MiddleButton;
	bound.at(16).mouse = input::MouseInput::None;
	expected.at(1).mouse = Mouse::Middle;
	expected.at(16).mouse = Mouse::None;
	ExpectSame(gui::bound_controls::Current(bound), expected);
}

// The page's bindings read back from a profile as they were kept; the zoom on keeps no button, as the game's
TEST(BoundControls, TheBindingsComeBackFromTheProfile)
{
	const auto kept = gui::bound_controls::ForProfile(k_Bindings);
	ASSERT_EQ(kept.size(), k_Bindings.size());
	EXPECT_EQ(kept.at(6), (player_profiles::KeyBinding {.key = 0x9D, .modifier = 0, .mouse = 0}));
	EXPECT_EQ(kept.at(2), (player_profiles::KeyBinding {.key = 0, .modifier = 0, .mouse = 5}));
	ExpectSame(gui::bound_controls::FromProfile(kept), k_Bindings);

	// The defaults, then each action the profile keeps; an action past the page's and a button past the game's are
	// passed over
	const std::map<uint32_t, player_profiles::KeyBinding> keys {
	    {13, {.key = 0x2D, .modifier = 0x1D, .mouse = 0}},
	    {1, {.key = 0, .modifier = 0, .mouse = 9}},
	    {99, {.key = 0x2C, .modifier = 0, .mouse = 1}},
	};
	auto expected = k_Bindings;
	expected.at(13).key = 0x2D;
	expected.at(13).modifier = 0x1D;
	expected.at(1).mouse = Mouse::None;
	ExpectSame(gui::bound_controls::FromProfile(keys), expected);
	ExpectSame(gui::bound_controls::FromProfile({}), k_Bindings);
}
