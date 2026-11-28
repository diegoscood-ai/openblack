/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::editor
{
/// While the editor is open, the testbed's scenarios window opening becomes the editor's Scenarios tab instead; with the
/// editor closed it opens as a window of its own
[[nodiscard]] constexpr bool ShowScenariosAsTab(bool editorOpen, bool scenariosWindowOpen)
{
	return editorOpen && scenariosWindowOpen;
}
} // namespace openblack::editor
