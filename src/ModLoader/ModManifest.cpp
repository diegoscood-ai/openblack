/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ModManifest.h"

#include <charconv>

#include <map>

namespace mod_loader
{
namespace
{
constexpr std::string_view k_Whitespace = " \t\r\n";

std::string_view Trim(std::string_view text)
{
	const auto first = text.find_first_not_of(k_Whitespace);
	if (first == std::string_view::npos)
	{
		return {};
	}
	return text.substr(first, text.find_last_not_of(k_Whitespace) - first + 1);
}

/// Calls `line` with each trimmed line that is neither blank nor a `#` comment
template <typename Fn>
void ForEachLine(std::string_view text, Fn&& line)
{
	while (!text.empty())
	{
		const auto end = text.find('\n');
		const auto current = Trim(text.substr(0, end));
		text = end == std::string_view::npos ? std::string_view {} : text.substr(end + 1);
		if (!current.empty() && current.front() != '#')
		{
			line(current);
		}
	}
}
} // namespace

std::optional<Manifest> ParseManifest(std::string_view text)
{
	std::map<std::string, std::string, std::less<>> values;
	bool first = true;
	bool formatFirst = false;
	ForEachLine(text, [&](std::string_view line) {
		const auto colon = line.find(':');
		if (colon == std::string_view::npos)
		{
			first = false;
			return;
		}
		const auto key = Trim(line.substr(0, colon));
		if (first)
		{
			formatFirst = key == "format";
			first = false;
		}
		values.try_emplace(std::string(key), std::string(Trim(line.substr(colon + 1))));
	});
	if (!formatFirst)
	{
		return std::nullopt;
	}
	const auto& format = values.at("format");
	int number = 0;
	const auto [end, error] = std::from_chars(format.data(), format.data() + format.size(), number);
	if (error != std::errc {} || end != format.data() + format.size() || number != k_ManifestFormat)
	{
		return std::nullopt;
	}
	const auto name = values.find("name");
	if (name == values.end() || name->second.empty())
	{
		return std::nullopt;
	}
	const auto valueOf = [&values](std::string_view key) {
		const auto found = values.find(key);
		return found == values.end() ? std::string {} : found->second;
	};
	return Manifest {.name = name->second, .version = valueOf("version"), .author = valueOf("author")};
}

std::set<std::string> ParseEnabled(std::string_view text)
{
	std::set<std::string> enabled;
	ForEachLine(text, [&enabled](std::string_view line) {
		if (IsModFolderName(line))
		{
			enabled.emplace(line);
		}
	});
	return enabled;
}

std::string WriteEnabled(const std::set<std::string>& enabled)
{
	std::string text;
	for (const auto& folder : enabled)
	{
		text += folder;
		text += '\n';
	}
	return text;
}

bool IsModFolderName(std::string_view name)
{
	return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\") == std::string_view::npos;
}

} // namespace mod_loader
