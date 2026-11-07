/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

namespace openblack::input
{

/// The number keys that are bookmark keys: 1 to 7. Eight bookmarks are drawn, but the key 8 is not one of them
[[nodiscard]] constexpr bool IsBookmarkKey(int number)
{
	return number >= 1 && number <= 7;
}

/// What a bookmark key does with the modifiers held: Ctrl alone drops the bookmark, Shift and Ctrl clear it, Shift
/// alone follows the thing it is on, the key alone flies to it, and with any other modifiers it does nothing
enum class BookmarkKeyAction : uint8_t
{
	None,
	Drop,
	Clear,
	Follow,
	FlyTo,
};

/// The modifiers' action: Shift with Ctrl clears (Alt or not); Ctrl without Alt drops; Shift follows; no modifier flies
[[nodiscard]] constexpr BookmarkKeyAction ActionOfBookmarkKey(bool shift, bool ctrl, bool alt)
{
	if (shift && ctrl)
	{
		return BookmarkKeyAction::Clear;
	}
	if (ctrl && !alt)
	{
		return BookmarkKeyAction::Drop;
	}
	if (shift)
	{
		return BookmarkKeyAction::Follow;
	}
	return !ctrl && !alt ? BookmarkKeyAction::FlyTo : BookmarkKeyAction::None;
}

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

/// Whether Backspace, back to the view before the last jump, acts: as the bookmark keys, but whatever the scripts have
/// the bookmarks at
[[nodiscard]] constexpr bool BeforeJumpKeyActs(BookmarkKeyGates gates)
{
	return !gates.scriptWideScreen && gates.keyShortcutsEnabled;
}

} // namespace openblack::input
