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
/// The editor owns the one Scripts panel. While the editor is open, the Scripts window opening becomes the editor's
/// Scripts tab instead; with the editor closed the window draws that same panel on its own
[[nodiscard]] constexpr bool ShowScriptsAsTab(bool editorOpen, bool scriptsWindowOpen)
{
	return editorOpen && scriptsWindowOpen;
}
} // namespace openblack::editor
