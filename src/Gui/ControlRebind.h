/*******************************************************************************
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

#include <optional>
#include <span>

#include "ControlBindings.h"
#include "Input/KeyBindings.h"

/// Binding an action of the Controls page anew, as the game's key binding box does, in the game's own key codes
/// (DirectInput's): which keys it takes, the modifier held with one, and how a new key or mouse button is taken off the
/// other actions.
namespace openblack::gui::control_rebind
{

/// The modifier keys' codes a binding keeps: the left one of each, whichever side is held
inline constexpr uint8_t k_Shift = 0x2A;
inline constexpr uint8_t k_Ctrl = 0x1D;
inline constexpr uint8_t k_Alt = 0x38;

/// Whether the box takes a key at all: not Escape, Tab, P, F1, F2 or the keypad's Enter
[[nodiscard]] bool IsBindableKey(uint8_t key) noexcept;

/// The modifier a key pressed is bound with, from the modifiers held as it is pressed: Shift when either Shift is held,
/// unless the key is a Shift; else Ctrl when either Ctrl is held, unless the key is a Ctrl; else Alt the same way; else
/// none (0). A Shift held with Ctrl gives Shift alone, and a Shift key pressed with Ctrl held gives no modifier
[[nodiscard]] uint8_t ModifierWith(uint8_t key, bool shift, bool ctrl, bool alt) noexcept;

/// An action given a key and its modifier (key 0: its key taken away): every other action with the same key and the
/// same modifier loses them first
void AssignKey(std::span<control_bindings::Binding> bindings, size_t action, uint8_t key, uint8_t modifier) noexcept;

/// An action given a mouse button (none: its button taken away): every action with the same button, itself among them,
/// loses it first
void AssignMouse(std::span<control_bindings::Binding> bindings, size_t action, control_bindings::Mouse mouse) noexcept;

/// The key chord an action of the page is bound to, for the game's action map: its key and modifier in SDL's codes;
/// none without a key, or for a key SDL has no code for
[[nodiscard]] std::optional<input::KeyChord> ChordOf(const control_bindings::Binding& binding) noexcept;
/// The mouse input of the page's button, for the game's action map; the page's picture of both buttons, which only
/// shows that a key zooms while they are held, is none
[[nodiscard]] input::MouseInput MouseInputOf(control_bindings::Mouse mouse) noexcept;

} // namespace openblack::gui::control_rebind
