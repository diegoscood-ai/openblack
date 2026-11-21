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
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

/// Finding a file whatever the case of its folders' and its own name, as the game's file system (which ignores case)
/// finds it, on a file system where case matters. Pure rules over the two lookups they are given, so that they are
/// tested without folders
namespace openblack::filesystem::case_path
{

/// Whether two names are the same once the case of their letters is ignored
[[nodiscard]] constexpr bool SameNameIgnoringCase(std::string_view left, std::string_view right)
{
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	return left.size() == right.size() &&
	       std::ranges::equal(left, right, [&lower](char l, char r) { return lower(l) == lower(r); });
}

/// The path on disk that `path` names whatever the case of its parts: itself when it exists as written; else, part by
/// part from its root, each part as written when that exists, or else the first name its folder lists that is the
/// same once case is ignored. None when a part has neither. `exists(path)` tells whether a path exists, and
/// `namesIn(folder)` lists the names in a folder (an empty folder path is the current folder)
template <typename Exists, typename NamesIn>
[[nodiscard]] std::optional<std::filesystem::path> FindIgnoringCase(const std::filesystem::path& path, Exists exists,
                                                                    NamesIn namesIn)
{
	if (path.empty())
	{
		return std::nullopt;
	}
	if (exists(path))
	{
		return path;
	}
	std::filesystem::path found = path.root_path();
	for (const auto& part : path.relative_path())
	{
		const std::string name = part.string();
		// an empty part (a trailing separator), "." and ".." are kept as they are
		if (name.empty() || name == "." || name == "..")
		{
			found /= part;
			continue;
		}
		if (auto asWritten = found / part; exists(asWritten))
		{
			found = std::move(asWritten);
			continue;
		}
		std::optional<std::string> match;
		for (const std::string& listed : namesIn(found))
		{
			if (SameNameIgnoringCase(listed, name))
			{
				match = listed;
				break;
			}
		}
		if (!match.has_value())
		{
			return std::nullopt;
		}
		found /= *match;
	}
	return found;
}

} // namespace openblack::filesystem::case_path
