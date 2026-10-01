/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <deque>
#include <filesystem>
#include <string>
#include <string_view>

namespace openblack::mods::log
{

/// What the mod library has to say (the Log tab of the Mods window); every message also goes to the "game" logger
enum class Level : uint8_t
{
	Info,
	Warning,
	Error,
};

struct Entry
{
	Level level {Level::Info};
	std::string mod; ///< the mod it is about, empty if none
	std::string text;
};

void Write(Level level, std::string_view mod, std::string_view text);
inline void Info(std::string_view mod, std::string_view text)
{
	Write(Level::Info, mod, text);
}
inline void Warning(std::string_view mod, std::string_view text)
{
	Write(Level::Warning, mod, text);
}
inline void Error(std::string_view mod, std::string_view text)
{
	Write(Level::Error, mod, text);
}

/// A path as UTF-8, whatever characters it has (path::string() throws on Windows when they do not fit the ANSI code
/// page, e.g. a mod folder with Japanese letters)
[[nodiscard]] std::string Utf8(const std::filesystem::path& path);

/// The last messages, oldest first (at most 2000; a host rule, not from the original)
[[nodiscard]] const std::deque<Entry>& Entries();
void Clear();

} // namespace openblack::mods::log
