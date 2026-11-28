/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Scripts.h"

#include "Editor/EditorWindow.h"
#include "Editor/ScriptsTab.h"

namespace openblack::debug::gui
{

Scripts::Scripts(editor::EditorWindow& editor) noexcept
    : Window("Scripts", ImVec2(1280.0f, 720.0f))
    , _editor(editor)
{
}

void Scripts::Draw() noexcept
{
	_editor.ScriptsTab().Draw();
}

void Scripts::Update() noexcept {}

void Scripts::UpdateAlways() noexcept
{
	if (editor::ShowScriptsAsTab(_editor.IsOpen(), IsOpen()))
	{
		Close();
		_editor.ShowScriptsTab();
	}
}

void Scripts::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void Scripts::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}

} // namespace openblack::debug::gui
