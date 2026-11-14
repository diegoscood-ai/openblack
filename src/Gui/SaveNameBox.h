/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include <glm/vec2.hpp>

#include "Common/Zoomer.h"
#include "Dialog.h"

namespace openblack::gui
{

class Control;
class EditBox;
class GameFont;
class TextDatabase;

/// The box of the temple's save game room that asks for the name of a save (refitted from raffclar's SaveGameBoxes).
///
/// A framed box of 500 by 250 in the middle of the dialog space: the room's "Save Game Message" across its top, the
/// name's box under it with the focus, then OK and Cancel. The name's box takes 32 characters and starts with the text
/// given: empty for a new save, the old save's name over another. Return in the name's box gives the focus to OK,
/// Return anywhere else is OK, Escape is Cancel. OK saves under the name; Cancel saves nothing. The box fades in over
/// half a second and out over a fifth.
class SaveNameBox
{
public:
	static constexpr DialogRect k_Box {.min = {150, 175}, .max = {650, 425}};
	static constexpr DialogRect k_Prompt {.min = {175, 200}, .max = {625, 250}};
	static constexpr DialogRect k_Name {.min = {175, 275}, .max = {625, 325}};
	static constexpr DialogRect k_Ok {.min = {250, 350}, .max = {350, 400}};
	static constexpr DialogRect k_Cancel {.min = {450, 350}, .max = {550, 400}};

	SaveNameBox(const TextDatabase& texts, const GameFont& font);
	SaveNameBox(const SaveNameBox&) = delete;
	SaveNameBox& operator=(const SaveNameBox&) = delete;
	~SaveNameBox();

	/// Brings the box up, the name's box holding the text given and the focus
	void Show(std::u16string_view name);
	/// Every dialog is put away: the box fades out as Cancel does
	void PutAway();
	/// Up and taking the input
	[[nodiscard]] bool IsActive() const noexcept { return _active; }
	[[nodiscard]] bool IsVisible() const noexcept { return _active || _fade.GetValue() > 0.0f; }
	/// The name in the name's box
	[[nodiscard]] std::u16string_view GetName() const;

	void MouseMove(glm::ivec2 point);
	void MouseDown(glm::ivec2 point);
	/// True when a control was let go over: the game clicks
	bool MouseUp(glm::ivec2 point);
	void TextInput(std::u16string_view text);
	void KeyDown(int key, bool shift);
	void Update(float deltaSeconds);
	void Draw(const DialogPainter& painter) const;

	/// The box's answer, once: the name for OK, none inside for Cancel
	[[nodiscard]] std::optional<std::optional<std::u16string>> TakeAnswer() { return std::exchange(_answer, std::nullopt); }

private:
	void Close(std::optional<std::u16string> answer);

	Dialog _dialog;
	EditBox* _name {nullptr};
	Control* _ok {nullptr};
	bool _active {false};
	Zoomer _fade;
	std::optional<std::optional<std::u16string>> _answer;
};

} // namespace openblack::gui
