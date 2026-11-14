/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScript.h"

#include <cctype>

#include <algorithm>

#include "LineReader.h"

namespace openblack::lhscriptx
{

namespace
{
constexpr std::string_view k_Remark = "REM";

[[nodiscard]] char Lower(char c) noexcept
{
	return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

[[nodiscard]] bool SameIgnoringCase(std::string_view a, std::string_view b) noexcept
{
	return std::ranges::equal(a, b, [](char x, char y) { return Lower(x) == Lower(y); });
}
} // namespace

std::vector<std::string> map_script::FeatureScripts(std::string_view source)
{
	std::vector<std::string> paths;
	line_reader::ForEachLine(source, [&paths](std::string_view line, int /*number*/) {
		line_reader::LineCursor cursor(line);
		while (true)
		{
			const auto word = cursor.Next();
			if (word.text.empty() && cursor.EndOfLine())
			{
				return true;
			}
			if (word.quoted)
			{
				continue;
			}
			// a remark: the rest of the line
			if (word.text == k_Remark)
			{
				return true;
			}
			if (word.text != k_LoadFeatureScript)
			{
				continue;
			}
			// its one argument, a text; a missing one stops the script as the reader does
			if (cursor.EndOfLine())
			{
				return false;
			}
			const auto path = cursor.Next();
			if (path.text.empty() && cursor.EndOfLine())
			{
				return false;
			}
			paths.push_back(path.text);
		}
	});
	return paths;
}

std::optional<std::string> map_script::InScriptsFolder(std::string_view path)
{
	// the folders of the path, '\' and '/' alike, a leading "." passed over
	std::vector<std::string_view> parts;
	size_t start = 0;
	for (size_t i = 0; i <= path.size(); ++i)
	{
		if (i == path.size() || path[i] == '\\' || path[i] == '/')
		{
			if (i > start)
			{
				parts.push_back(path.substr(start, i - start));
			}
			start = i + 1;
		}
	}
	if (!parts.empty() && parts.front() == ".")
	{
		parts.erase(parts.begin());
	}
	if (parts.size() != 2 || !SameIgnoringCase(parts.front(), "scripts"))
	{
		return std::nullopt;
	}
	return std::string(parts.back());
}

std::optional<std::string> map_script::MatchingName(std::string_view name, std::span<const std::string> names)
{
	const auto found =
	    std::ranges::find_if(names, [name](const std::string& candidate) { return SameIgnoringCase(candidate, name); });
	if (found == names.end())
	{
		return std::nullopt;
	}
	return *found;
}

} // namespace openblack::lhscriptx
