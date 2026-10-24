/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::input
{

/// What keeps the camera bookmark keys (1 to 8) from acting
struct BookmarkKeyGates
{
	/// A script holds the cinema bars
	bool scriptWideScreen;
	/// The script's interface interaction leaves the key shortcuts on
	bool keyShortcutsEnabled;
	/// The scripts leave the bookmarks on (PLAY_JC_SPECIAL 14 turns them on, 15 off)
	bool bookmarksEnabled;
};

/// Whether a bookmark key, to set a bookmark or to fly to one, acts: not while a script holds the cinema bars, nor
/// while the interface interaction has the key shortcuts off, nor while the scripts have the bookmarks off
[[nodiscard]] constexpr bool BookmarkKeyActs(BookmarkKeyGates gates)
{
	return !gates.scriptWideScreen && gates.keyShortcutsEnabled && gates.bookmarksEnabled;
}

} // namespace openblack::input
