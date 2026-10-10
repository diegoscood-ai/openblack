/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

namespace openblack::string_utils
{

[[nodiscard]] bool EndsWith(const std::string& str, const std::string& ending);

[[nodiscard]] bool BeginsWith(const std::string& str, const std::string& beginning);

[[nodiscard]] std::string UpperCase(const std::string& str);

[[nodiscard]] std::string LowerCase(const std::string& str);

[[nodiscard]] std::string Capitalise(const std::string& str);

[[nodiscard]] std::vector<std::string> Split(const std::string& string, const std::string& delimiter);

/// Extract a substring of the characters in between the first two quote of a string
[[nodiscard]] std::string ExtractQuote(std::string& string);

/// A source file's path (as std::source_location gives it) from inside the source tree: everything up to and including
/// the last "src" folder goes, whichever the separators. A path without that folder stays whole
[[nodiscard]] constexpr std::string_view SourceRelativePath(std::string_view path) noexcept
{
	constexpr std::string_view k_Folder = "src";
	constexpr auto isSeparator = [](char c) { return c == '/' || c == '\\'; };
	for (auto at = path.rfind(k_Folder); at != std::string_view::npos && at > 0; at = path.rfind(k_Folder, at - 1))
	{
		const auto end = at + k_Folder.size();
		if (isSeparator(path[at - 1]) && end < path.size() && isSeparator(path[end]))
		{
			return path.substr(end + 1);
		}
	}
	return path;
}

} // namespace openblack::string_utils
