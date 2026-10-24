/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FileDialogWindow.h"

#include <imgui.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Windowing/WindowingInterface.h"

using namespace openblack;
using namespace openblack::debug::gui;

FileDialogWindow::FileDialogWindow() noexcept
    : Window("File Dialog", ImVec2(480.0f, 200.0f))
{
}

file_dialog::Request FileDialogWindow::MakeRequest() const
{
	return {
	    .mode = _mode,
	    .title = _mode == file_dialog::Mode::Open ? "Choose a file" : "Choose a file name",
	    .filters = {{.name = "All files", .patterns = {"*"}}},
	    // The game's folder, as a place to start from
	    .startFolder = Locator::filesystem::has_value() ? Locator::filesystem::value().GetGamePath() : std::filesystem::path {},
	    .defaultName = _mode == file_dialog::Mode::Save ? "untitled" : "",
	    .owner = Locator::windowing::has_value() ? Locator::windowing::value().GetNativeHandles().nativeWindow : nullptr,
	};
}

void FileDialogWindow::Draw() noexcept
{
	if (ImGui::RadioButton("Open", _mode == file_dialog::Mode::Open))
	{
		_mode = file_dialog::Mode::Open;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Save", _mode == file_dialog::Mode::Save))
	{
		_mode = file_dialog::Mode::Save;
	}

	if (ImGui::Button("Platform dialog..."))
	{
		const auto request = MakeRequest();
		const auto outcome = file_dialog::Show(request);
		switch (outcome.status)
		{
		case file_dialog::Status::Chosen:
			_result = "Chosen: " + outcome.path.generic_string();
			break;
		case file_dialog::Status::Cancelled:
			_result = "Cancelled";
			break;
		case file_dialog::Status::Unavailable:
			_result = "No dialog on this platform: the debug browser stands in";
			_browser.Open(request);
			break;
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Debug browser..."))
	{
		_browser.Open(MakeRequest());
	}

	if (const auto chosen = _browser.Draw())
	{
		_result = "Chosen: " + chosen->generic_string();
	}
	ImGui::TextWrapped("%s", _result.c_str());
}

void FileDialogWindow::Update() noexcept {}

void FileDialogWindow::ProcessEventOpen([[maybe_unused]] const SDL_Event& event) noexcept {}

void FileDialogWindow::ProcessEventAlways([[maybe_unused]] const SDL_Event& event) noexcept {}
