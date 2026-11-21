/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SaveNameBox.h"

#include <algorithm>

#include <SDL_keycode.h>

#include "Controls.h"
#include "DialogKeyRules.h"
#include "SaveNameBoxRules.h"
#include "Temple/SaveGameRoomRules.h"
#include "TextDatabase.h"

using namespace openblack::gui;

namespace
{
constexpr float k_FadeInSeconds = 0.5f;
constexpr float k_FadeOutSeconds = 0.2f;

constexpr std::string_view k_PromptText = "HELP_TEXT_ROOM_CITADEL_SAVEGAME_ROOM_SAVEMESSAGE";
constexpr std::string_view k_OkText = "HELP_TEXT_REQUESTER_BOXES_04";
constexpr std::string_view k_CancelText = "HELP_TEXT_REQUESTER_BOXES_03";
/// What the table gives for a text it doesn't have
constexpr std::string_view k_MissingText = "HELP_TEXT_NONE";

std::u16string Text(const TextDatabase& texts, std::string_view name)
{
	const auto text = texts.Get(name);
	return std::u16string(text.empty() ? texts.Get(k_MissingText) : text);
}
} // namespace

SaveNameBox::SaveNameBox(const TextDatabase& texts, const GameFont& font)
{
	// Made in the order the game makes them: the prompt, OK, Cancel and the name's box. The prompt takes the mouse,
	// clicking but doing nothing
	auto& prompt = _dialog.Add<StaticText>(k_Prompt, Text(texts, k_PromptText), StaticText::Layout::Wrapped);
	prompt.takesClicks = true;
	auto& ok = _dialog.Add<Button>(k_Ok, Text(texts, k_OkText));
	ok.onClick = [this] { Close(std::u16string(GetName())); };
	_ok = &ok;
	_dialog.Add<Button>(k_Cancel, Text(texts, k_CancelText)).onClick = [this] { Close(std::nullopt); };
	_name = &_dialog.Add<EditBox>(font, k_Name, std::u16string {}, temple::save_game_room::k_NameLength,
	                              DialogPainter::k_BigTextSize);
}

SaveNameBox::~SaveNameBox() = default;

void SaveNameBox::Show(std::u16string_view name)
{
	_active = true;
	_answer.reset();
	_dialog.Reset();
	_name->SetText(name.substr(0, temple::save_game_room::k_NameLength));
	_dialog.SetFocus(_name);
	_fade.SetPosition(0.0f);
	_fade.SetDestinationWithSpeedAndTime(1.0f, 0.0f, k_FadeInSeconds);
}

void SaveNameBox::PutAway()
{
	if (_active)
	{
		Close(std::nullopt);
	}
}

void SaveNameBox::Close(std::optional<std::u16string> answer)
{
	_active = false;
	_dialog.Reset();
	_fade.SetDestinationWithSpeedAndTime(0.0f, 0.0f, k_FadeOutSeconds);
	_answer.emplace(std::move(answer));
}

std::u16string_view SaveNameBox::GetName() const
{
	return _name->GetText();
}

void SaveNameBox::MouseMove(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseMove(point);
	}
}

void SaveNameBox::MouseDown(glm::ivec2 point)
{
	if (_active)
	{
		_dialog.MouseDown(point);
	}
}

bool SaveNameBox::MouseUp(glm::ivec2 point)
{
	return _active && _dialog.MouseUp(point);
}

void SaveNameBox::TextInput(std::u16string_view text)
{
	if (_active)
	{
		_dialog.TextInput(text);
	}
}

void SaveNameBox::KeyDown(int key, bool shift)
{
	if (!_active)
	{
		return;
	}
	switch (save_name_box::StepOfKey(dialog_keys::IsReturn(key), key == SDLK_ESCAPE, _dialog.GetFocus() == _name))
	{
	case save_name_box::KeyStep::FocusOk:
		_dialog.SetFocus(_ok);
		break;
	case save_name_box::KeyStep::Save:
		Close(std::u16string(GetName()));
		break;
	case save_name_box::KeyStep::Cancel:
		Close(std::nullopt);
		break;
	case save_name_box::KeyStep::ToControl:
		_dialog.KeyDown(key, shift);
		break;
	}
}

void SaveNameBox::Update(float deltaSeconds)
{
	_fade.Update(deltaSeconds);
	if (_active)
	{
		_dialog.Update(deltaSeconds);
	}
}

void SaveNameBox::Draw(const DialogPainter& painter) const
{
	if (!IsVisible())
	{
		return;
	}
	const auto alpha = std::clamp(_fade.GetValue(), 0.0f, 1.0f);
	painter.SetAlpha(alpha);
	painter.DrawBackground(k_Box, glm::vec3(1.0f), false, DialogPainter::Edges::All);
	painter.SetAlpha(alpha);
	// Fading out, nothing lights up
	_dialog.Draw(painter, _active);
	painter.SetAlpha(1.0f);
}
