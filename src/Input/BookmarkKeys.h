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

#include <optional>

#include <SDL_scancode.h>

namespace openblack::input
{

/// The number keys that are bookmark keys: 1 to 7. Eight bookmarks are drawn, but the key 8 is not one of them
[[nodiscard]] constexpr bool IsBookmarkKey(int number)
{
	return number >= 1 && number <= 7;
}

/// The bookmark (0 to 6) of a key read by its place on the keyboard, the top row's 1 to 7 whatever the layout gives
/// them (a French keyboard's & to è), as the original reads its keys; none for any other key, the keypad's included
[[nodiscard]] constexpr std::optional<uint8_t> BookmarkOfScancode(SDL_Scancode scancode)
{
	if (scancode < SDL_SCANCODE_1 || scancode > SDL_SCANCODE_0)
	{
		return std::nullopt;
	}
	const int number = static_cast<int>(scancode - SDL_SCANCODE_1) + 1;
	if (!IsBookmarkKey(number))
	{
		return std::nullopt;
	}
	return static_cast<uint8_t>(number - 1);
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
	/// The player is inside the temple, which takes every key itself
	bool insideTemple;
};

/// Whether a bookmark key, to set a bookmark, clear it, follow it or fly to it, acts: not inside the temple, which takes
/// every key, nor while a script holds the cinema bars, nor while the interface interaction has the key shortcuts off,
/// nor while the scripts have the bookmarks off
[[nodiscard]] constexpr bool BookmarkKeyActs(BookmarkKeyGates gates)
{
	return !gates.insideTemple && !gates.scriptWideScreen && gates.keyShortcutsEnabled && gates.bookmarksEnabled;
}

/// Whether Backspace, back to the view before the last jump, acts: as the bookmark keys, but whatever the scripts have
/// the bookmarks at
[[nodiscard]] constexpr bool BeforeJumpKeyActs(BookmarkKeyGates gates)
{
	return !gates.scriptWideScreen && gates.keyShortcutsEnabled;
}

} // namespace openblack::input
