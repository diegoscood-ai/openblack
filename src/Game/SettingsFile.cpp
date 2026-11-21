/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SettingsFile.h"

#include <algorithm>
#include <optional>

namespace openblack::settings_file
{
namespace
{
constexpr std::string_view k_Separators = " ,\t\n";

/// The next word of a line from `at`, and where the reading stops after it; none when only separators are left
std::optional<std::string_view> NextWord(std::string_view line, size_t& at)
{
	const auto start = line.find_first_not_of(k_Separators, at);
	if (start == std::string_view::npos)
	{
		at = line.size();
		return std::nullopt;
	}
	auto end = line.find_first_of(k_Separators, start);
	if (end == std::string_view::npos)
	{
		end = line.size();
	}
	at = end < line.size() ? end + 1 : end;
	return line.substr(start, end - start);
}
} // namespace

std::string QuestFolder(std::string_view text)
{
	// read as a text file: a Ctrl+Z ends it, and a carriage return before a line feed is dropped
	text = text.substr(0, text.find('\x1A'));
	std::string read;
	read.reserve(text.size());
	for (size_t i = 0; i < text.size(); ++i)
	{
		if (text[i] == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
		{
			continue;
		}
		read.push_back(text[i]);
	}

	std::string folder(k_DefaultQuestFolder);
	const std::string_view all(read);
	size_t from = 0;
	while (from < all.size())
	{
		// one read: up to and with the line's end, at most k_LinePiece characters
		auto length = std::min(all.size() - from, k_LinePiece);
		if (const auto end = all.substr(from, length).find('\n'); end != std::string_view::npos)
		{
			length = end + 1;
		}
		const auto line = all.substr(from, length);
		from += length;

		size_t at = 0;
		const auto first = NextWord(line, at);
		if (!first.has_value() || first->front() == '/' || (first->size() > 1 && (*first)[1] == '/'))
		{
			continue;
		}
		if (*first != k_QuestFolderWord)
		{
			continue;
		}
		if (const auto value = NextWord(line, at); value.has_value())
		{
			folder = *value;
		}
	}
	return folder;
}

} // namespace openblack::settings_file
