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
#include <string_view>

/// How the keyboard works the controls of the game's dialogs: which control has the focus after Tab, and how a slider
/// or a list moves for a key
namespace openblack::gui::dialog_keys
{

/// The control that takes the focus after Tab, or Shift and Tab when backwards. The controls are in the order of Tab,
/// each true when it can take the focus: it is visible and it is not a static text. Tab goes to the next one that can
/// take the focus, after the last one to the first, and with no focus to the first. Backwards it goes the other way,
/// and with no focus to the last. When no other control can take it, the focus stays. None when no control has the
/// focus and none can take it.
[[nodiscard]] std::optional<size_t> NextFocus(std::span<const bool> canFocus, std::optional<size_t> focus, bool backwards);

/// A slider moved by a key: Home puts it at the start, End at the end, and the left and right arrows step it a tenth.
/// The value stays from 0 to 1.
enum class SliderKey
{
	Home,
	End,
	Left,
	Right,
};
[[nodiscard]] float MoveSlider(float value, SliderKey key);

/// A list moved by a key: Home picks the first item, End the last, and the up and down arrows the one before or after.
/// Up from the first item, or from none, picks the last; down from the last picks the first. None when the list is
/// empty.
enum class ListKey
{
	Home,
	End,
	Up,
	Down,
};
[[nodiscard]] std::optional<size_t> MoveList(std::optional<size_t> selection, size_t count, ListKey key);

/// Return, on the keyboard or the keypad
[[nodiscard]] bool IsReturn(int keyCode);

/// What a letter typed answers in a question with a Yes and a No
enum class Answer : uint8_t
{
	None,
	Yes,
	No,
};
/// A letter typed over a question: the first letter of the Yes text answers yes, of the No text no, in either case (only
/// the letters a to z have two); nothing when the two texts start alike, or for a letter of neither
[[nodiscard]] Answer AnswerOfLetter(char16_t typed, std::u16string_view yes, std::u16string_view no);

} // namespace openblack::gui::dialog_keys
