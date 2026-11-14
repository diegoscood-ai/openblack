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
#include <string>

/// The names the game shows for its keys are the operating system's, in the language of its keyboard layout: the
/// game asks the system for each key's name as it binds the key, with room for 15 letters, so a longer name is cut
/// short there. Left and right Shift and Ctrl give one name, the system being told not to tell them apart.
namespace openblack::input::system_key_name
{

/// The room the game gives a key's name, its end included
inline constexpr int32_t k_NameRoom = 16;

/// What the game asks the system for, for a DirectInput key code: the key's scan code at bit 16, the extended key
/// bit (the code's 0x80) at bit 24 and the bit that makes no difference between the left and right keys at bit 25
[[nodiscard]] constexpr int32_t NameParam(uint8_t code) noexcept
{
	return static_cast<int32_t>((((code & 0x80u) | 0x100u) << 1u | (code & 0x7Fu)) << 16u);
}

/// Where the names come from: the parameter above and the room, the name the system gives (empty for none)
using Source = std::u16string (*)(int32_t param, int32_t room);

/// Whether the run reads the real input
using RealInputQuery = bool (*)();

/// The names the game shows, installed as it starts: while the run reads the real input, the system's own (on systems
/// that have them, Windows; elsewhere nothing changes); in a run that reads none (a test run: the real input ignored,
/// a fixed or pinned mouse, a recording playing), the English (United States) layout's, so that a test run names the
/// keys the same on every machine, as its mouse is the same (MouseOfRun). Asked at each name
void InstallSystemNames(RealInputQuery realInput) noexcept;
/// Tests: the names come from here (nullptr: no names, as before the game installs the system's)
void SetSource(Source source) noexcept;

/// The English (United States) layout's name of a DirectInput key code, empty for a code it has no name for
[[nodiscard]] std::u16string_view EnglishUsName(uint8_t code) noexcept;

/// The name the game shows for a DirectInput key code, as the installed names give it; none while none are installed,
/// or with the real input on a system with no names of its own
[[nodiscard]] std::optional<std::u16string> Name(uint8_t code);

} // namespace openblack::input::system_key_name
