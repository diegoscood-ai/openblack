/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string>

#include "Common/FileDialog.h"
#include "FileBrowser.h"
#include "Window.h"

namespace openblack::debug::gui
{

/// Tries out the file dialog: a button asks for a file with the platform's dialog, falling back to the debug file
/// browser where there is none, and another opens the browser itself. It only shows the path chosen: nothing is read,
/// written or remembered, and nothing happens until a button is pressed.
class FileDialogWindow final: public Window
{
public:
	FileDialogWindow() noexcept;

protected:
	void Draw() noexcept override;
	void Update() noexcept override;
	void ProcessEventOpen(const SDL_Event& event) noexcept override;
	void ProcessEventAlways(const SDL_Event& event) noexcept override;

private:
	[[nodiscard]] file_dialog::Request MakeRequest() const;

	file_dialog::Mode _mode {file_dialog::Mode::Open};
	FileBrowser _browser;
	/// What became of the last request
	std::string _result;
};

} // namespace openblack::debug::gui
