/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <SDL_scancode.h>

/// The game's key codes, DirectInput's (the keyboard's set 1 scan codes, 0x80 added for the extended keys), and the
/// SDL scan codes of the same keys
namespace openblack::input::direct_input
{

/// The SDL key of a DirectInput key code; none for a code no key of a PC keyboard has
[[nodiscard]] std::optional<SDL_Scancode> ScancodeOf(uint8_t code) noexcept;
/// The DirectInput key code of an SDL key; none for a key DirectInput has no code for
[[nodiscard]] std::optional<uint8_t> CodeOf(SDL_Scancode key) noexcept;

} // namespace openblack::input::direct_input
