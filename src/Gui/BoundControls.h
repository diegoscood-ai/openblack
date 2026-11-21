/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <map>
#include <span>

#include "Common/PlayerProfiles.h"
#include "ControlBindings.h"
#include "Input/KeyBindings.h"

/// The game's controls as they are bound now, in the Controls page's terms: the page's rows of the action map's
/// bindings, and the bindings a profile keeps
namespace openblack::gui::bound_controls
{

using Bindings = std::array<control_bindings::Binding, control_bindings::k_Bindings.size()>;

/// The Controls page's rows of the action map's bindings, in the page's order, which is the action map's: each
/// action's key and modifier in the game's key codes (none for a key the game has no code for) and its mouse button, so
/// the defaults give the page's defaults
[[nodiscard]] Bindings Current(std::span<const input::KeyBinding> bound);

/// The page's bindings a profile keeps, as the page is left: every action's key, modifier and button in the game's
/// codes
[[nodiscard]] std::map<uint32_t, player_profiles::KeyBinding> ForProfile(std::span<const control_bindings::Binding> bindings);
/// The page's bindings of a profile: the defaults, then each action the profile keeps a binding for takes it as it is
/// kept (a button past the game's is none)
[[nodiscard]] Bindings FromProfile(const std::map<uint32_t, player_profiles::KeyBinding>& keys);

} // namespace openblack::gui::bound_controls
