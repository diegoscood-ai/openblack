/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <filesystem>
#include <optional>
#include <span>

/// Where a data mod's files replace the game's: each switched-on mod's folder is a search root, looked in before the
/// game's own folder, for the same path below it. Pure rules, so that they are tested without folders
namespace openblack::filesystem::mod_roots
{

/// The path below the game's folder that a game data path names: itself when it is relative, its part below the game
/// folder when it is inside it; none for a path elsewhere, which no mod replaces
[[nodiscard]] inline std::optional<std::filesystem::path> GameRelative(const std::filesystem::path& path,
                                                                       const std::filesystem::path& gamePath)
{
	if (path.empty())
	{
		return std::nullopt;
	}
	if (path.is_relative())
	{
		return path;
	}
	if (gamePath.empty())
	{
		return std::nullopt;
	}
	const auto relative = path.lexically_normal().lexically_relative(gamePath.lexically_normal());
	if (relative.empty() || relative.is_absolute() || *relative.begin() == "..")
	{
		return std::nullopt;
	}
	return relative;
}

/// The first root, in the roots' order, that holds `relative`, as the full path; none when no root holds it
template <typename Exists>
[[nodiscard]] std::optional<std::filesystem::path> FirstInRoots(std::span<const std::filesystem::path> roots,
                                                                const std::filesystem::path& relative, Exists exists)
{
	for (const auto& root : roots)
	{
		auto candidate = root / relative;
		if (exists(candidate))
		{
			return candidate;
		}
	}
	return std::nullopt;
}

} // namespace openblack::filesystem::mod_roots
