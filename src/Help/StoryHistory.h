/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <functional>
#include <string_view>

namespace openblack::help
{

struct HistoryEntry;

/// A view of the help texts that the scripts showed, which the library's last scroll shows. The dialogue keeps the
/// history; the view only reads it.
struct StoryHistoryView
{
	/// The line some lines back from the newest line, 0 for the newest line. Null past the oldest line.
	std::function<const HistoryEntry*(int32_t linesBack)> back;
	/// How many lines the history keeps
	int32_t count {0};

	[[nodiscard]] const HistoryEntry* Back(int32_t linesBack) const { return back ? back(linesBack) : nullptr; }
};

/// The buttons of the story history in the library turn the first line that it shows
enum class StoryTurn
{
	/// One line further back
	Previous,
	/// One line nearer to the newest line
	Next,
	/// Back to the newest line
	Latest,
};

/// The text that comes before a line in the library: who said it, by the narrator of the line
[[nodiscard]] std::string_view StoryCaption(int32_t narrator);

/// The first line that the library shows, kept from 0 (the newest line) to the oldest line
[[nodiscard]] int32_t ClampStoryStart(int32_t start, size_t count);

} // namespace openblack::help
