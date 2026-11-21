/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VersionFile.h"

#include <algorithm>
#include <numeric>
#include <ranges>

namespace openblack::version_file
{

namespace
{
/// A letter as the game's C library lowers it: only A to Z
constexpr char Lower(char c)
{
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

constexpr bool StartsSwitch(char c)
{
	return c == '-' || c == '/';
}
} // namespace

bool NamesSwitch(std::string_view from, std::string_view name)
{
	if (from.size() < name.size())
	{
		return false;
	}
	return std::ranges::equal(from.substr(0, name.size()), name, {}, Lower, Lower);
}

bool HasSwitch(std::string_view commandLine, std::string_view name)
{
	for (size_t i = 0; i < commandLine.size(); ++i)
	{
		if (StartsSwitch(commandLine[i]) && NamesSwitch(commandLine.substr(i + 1), name))
		{
			return true;
		}
	}
	return false;
}

std::string CommandLine(std::span<const std::string_view> arguments)
{
	std::string line;
	for (const auto argument : arguments)
	{
		if (!line.empty())
		{
			line.push_back(' ');
		}
		line += argument;
	}
	return line;
}

std::vector<std::string_view> WithoutSwitch(std::span<const std::string_view> arguments, std::string_view name)
{
	std::vector<std::string_view> kept;
	for (const auto argument : arguments)
	{
		if (!argument.empty() && StartsSwitch(argument.front()) && NamesSwitch(argument.substr(1), name))
		{
			continue;
		}
		kept.push_back(argument);
	}
	return kept;
}

uint32_t Checksum(std::span<const uint8_t> program)
{
	return std::accumulate(program.begin(), program.end(), uint32_t {0}, [](uint32_t sum, uint8_t byte) { return sum + byte; });
}

std::string Text(uint32_t checksum)
{
	return std::to_string(checksum);
}

} // namespace openblack::version_file
