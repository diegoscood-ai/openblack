/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <string>
#include <string_view>

/// The game's controls as its Controls page and its help name them: each action's key and mouse button
namespace openblack::gui::control_bindings
{

/// The mouse buttons an action can be bound to
enum class Mouse
{
	None,
	Left,
	Middle,
	WheelUp,
	WheelDown,
	Right,
	/// Both buttons, for the actions that zoom while a key is held
	Both,
};

/// The default bindings for a mouse with a wheel: the actions in the order the Controls page lists them, their
/// names, and the DirectInput key and mouse button bound to them
struct Binding
{
	std::string_view name;
	uint8_t key;
	uint8_t modifier;
	Mouse mouse;
};
inline constexpr std::array k_Bindings = {
    Binding {.name = "HELP_TEXT_ACTIONS_HELP", .key = 0x3B, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_SELECT", .key = 0, .modifier = 0, .mouse = Mouse::Left},
    Binding {.name = "HELP_TEXT_ACTIONS_APPLY", .key = 0, .modifier = 0, .mouse = Mouse::Right},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMOUT", .key = 0, .modifier = 0, .mouse = Mouse::WheelDown},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMIN", .key = 0, .modifier = 0, .mouse = Mouse::WheelUp},
    Binding {.name = "HELP_TEXT_ACTIONS_TALK", .key = 0x14, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMON", .key = 0x9D, .modifier = 0, .mouse = Mouse::Both},
    Binding {.name = "HELP_TEXT_ACTIONS_MOVELEFT", .key = 0xCB, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_MOVERIGHT", .key = 0xCD, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_MOVEFORWARD", .key = 0xC8, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_MOVEBACKWARD", .key = 0xD0, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_TILTUP", .key = 0x1E, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_TILTDOWN", .key = 0x10, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ROTATELEFT", .key = 0x2C, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ROTATERIGHT", .key = 0x16, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_TILTROTATEON", .key = 0x36, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_TILTROTATEAROUNDMOUSE", .key = 0, .modifier = 0, .mouse = Mouse::Middle},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_CITADEL", .key = 0x39, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_CREATURE", .key = 0x2E, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_REALM", .key = 0x3D, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_INSIDECITADEL", .key = 0x3E, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_CREATUREROOM", .key = 0x3F, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_CHALLENGEROOM", .key = 0x40, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_SAVEGAMEROOM", .key = 0x41, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_OPTIONSROOM", .key = 0x42, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_ZOOMTO_MULTIPLAYERROOM", .key = 0x43, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_LEASH", .key = 0x26, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_SHOWVILLAGERNAME", .key = 0x31, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_SHOWVILLAGERDETAILS", .key = 0x1F, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_TOOLTIP_155", .key = 0x1F, .modifier = 0x1D, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_TOOLTIP_154", .key = 0x26, .modifier = 0x1D, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_PREVIOUS_LEASH", .key = 0x2F, .modifier = 0, .mouse = Mouse::None},
    Binding {.name = "HELP_TEXT_ACTIONS_NEXT_LEASH", .key = 0x30, .modifier = 0, .mouse = Mouse::None},
};

/// The words the game shows for a key, as GetKeyNameText gives them
[[nodiscard]] std::u16string KeyName(uint8_t key);
/// The words the game shows for an action's key: its modifier first, " + ", then the key; empty for no key
[[nodiscard]] std::u16string KeyText(const Binding& binding);
/// The name in the game's texts of a mouse button, empty for none
[[nodiscard]] std::string_view MouseTextName(Mouse mouse) noexcept;

} // namespace openblack::gui::control_bindings
