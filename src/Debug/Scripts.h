/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "Window.h"

namespace openblack::editor
{
class EditorWindow;
}

namespace openblack::debug::gui
{

/// The loaded script program, to read and to debug: the editor's Scripts panel, the same one, in a window of its own
/// while the editor is closed. Opened while the editor is open, it closes and the editor shows its Scripts tab. Closed,
/// it does nothing; open, it changes the scripts only through the panel's own controls.
class Scripts final: public Window
{
public:
	explicit Scripts(editor::EditorWindow& editor) noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void UpdateAlways() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	editor::EditorWindow& _editor;
};

} // namespace openblack::debug::gui
