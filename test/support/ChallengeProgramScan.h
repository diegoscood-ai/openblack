/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

// The shipped challenge program's families and the script natives' source, as the challenge program's tests read
// them: the program under OPENBLACK_GAME_PATH, the natives' bindings and bodies in src/CHLApi.cpp (from the source
// folder, OPENBLACK_SOURCE_DIR, which the including test is built with), which natives are unwritten, and the families
// the progress page lists as running whole

#include <cctype>
#include <cstdint>
#include <cstdlib>

#include <array>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <LHVMFile.h>

namespace openblack::test::challenge_scan
{

/// The challenge program of the game OPENBLACK_GAME_PATH points at, when it has one
inline std::optional<std::filesystem::path> ChallengePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	auto path = std::filesystem::path(game) / "Scripts" / "Quests" / "challenge.chl";
	if (!std::filesystem::exists(path))
	{
		return std::nullopt;
	}
	return path;
}

/// The source folder, from OPENBLACK_SOURCE_DIR when set (a test built in another tree), else the one built from
inline std::filesystem::path SourceDir()
{
	if (const auto* dir = std::getenv("OPENBLACK_SOURCE_DIR"); dir != nullptr && *dir != '\0')
	{
		return dir;
	}
	return OPENBLACK_SOURCE_DIR;
}

inline std::string ReadText(const std::filesystem::path& path)
{
	std::ifstream stream(path, std::ios::binary);
	std::ostringstream text;
	text << stream.rdbuf();
	return text.str();
}

/// A source file's lines, without their line ends
inline std::vector<std::string> Lines(std::string_view text)
{
	std::vector<std::string> lines;
	std::istringstream stream {std::string(text)};
	for (std::string line; std::getline(stream, line);)
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}
		lines.push_back(std::move(line));
	}
	return lines;
}

[[nodiscard]] inline bool IsIdentifierChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

/// The native table as the source binds it, in order: each native's name and the function that runs it
inline std::vector<std::pair<std::string, std::string>> Bindings(std::string_view source)
{
	constexpr std::string_view k_Binding = "CREATE_FUNCTION_BINDING(\"";
	std::vector<std::pair<std::string, std::string>> bindings;
	for (auto at = source.find(k_Binding); at != std::string_view::npos; at = source.find(k_Binding, at + 1))
	{
		const auto nameStart = at + k_Binding.size();
		const auto nameEnd = source.find('"', nameStart);
		// the stack counts come next; the function is the last argument, perhaps on the next line
		auto comma = nameEnd;
		for (int i = 0; i < 3 && comma != std::string_view::npos; ++i)
		{
			comma = source.find(',', comma + 1);
		}
		if (nameEnd == std::string_view::npos || comma == std::string_view::npos)
		{
			break;
		}
		auto start = comma + 1;
		while (start < source.size() && std::isspace(static_cast<unsigned char>(source[start])) != 0)
		{
			++start;
		}
		auto end = start;
		while (end < source.size() && IsIdentifierChar(source[end]))
		{
			++end;
		}
		bindings.emplace_back(std::string(source.substr(nameStart, nameEnd - nameStart)),
		                      std::string(source.substr(start, end - start)));
	}
	return bindings;
}

/// The bodies of the source's functions defined at the start of a line, by name: a body runs from a "{" line to the
/// next "}" line, or is the rest of a definition written on one line
inline std::vector<std::pair<std::string, std::string>> FunctionBodies(std::string_view source)
{
	const auto lines = Lines(source);
	std::vector<std::pair<std::string, std::string>> bodies;
	for (size_t i = 0; i < lines.size(); ++i)
	{
		const auto& line = lines[i];
		const auto open = line.find('(');
		if (line.empty() || std::isalpha(static_cast<unsigned char>(line[0])) == 0 || open == std::string::npos)
		{
			continue;
		}
		auto nameStart = open;
		while (nameStart > 0 && IsIdentifierChar(line[nameStart - 1]))
		{
			--nameStart;
		}
		const auto name = line.substr(nameStart, open - nameStart);
		if (name.empty())
		{
			continue;
		}
		if (i + 1 < lines.size() && lines[i + 1] == "{")
		{
			std::string body;
			size_t j = i + 2;
			for (; j < lines.size() && lines[j] != "}"; ++j)
			{
				body += lines[j];
				body += '\n';
			}
			bodies.emplace_back(name, std::move(body));
			i = j;
		}
		else if (const auto brace = line.find('{', open); brace != std::string::npos)
		{
			if (line.find('}', brace) != std::string::npos)
			{
				bodies.emplace_back(name, line.substr(brace));
			}
		}
	}
	return bodies;
}

/// The call an unwritten native makes, with no case
constexpr std::string_view k_UnwrittenCall = "NotImplemented()";
/// The delete, which has every path written
constexpr std::string_view k_DeleteNative = "OBJECT_DELETE";

/// The natives, by number, whose own body logs "not implemented" with no case, and those the source binds to no
/// function it defines
inline std::set<uint32_t> UnwrittenNatives(std::string_view source)
{
	const auto bindings = Bindings(source);
	const auto bodies = FunctionBodies(source);
	std::set<uint32_t> unwritten;
	for (uint32_t native = 0; native < bindings.size(); ++native)
	{
		const auto& function = bindings[native].second;
		bool defined = false;
		bool logs = false;
		for (const auto& [bodyName, body] : bodies)
		{
			if (bodyName == function)
			{
				defined = true;
				logs = logs || body.find(k_UnwrittenCall) != std::string::npos;
			}
		}
		if (!defined || logs)
		{
			unwritten.insert(native);
		}
	}
	return unwritten;
}

/// A file name as the progress page writes it: letters and digits only, in lower case
inline std::string Folded(std::string_view text)
{
	std::string folded;
	for (const char c : text)
	{
		if (std::isalnum(static_cast<unsigned char>(c)) != 0)
		{
			folded += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
	}
	return folded;
}

/// The source files whose name, without its extension, is the family's name as the page writes it
inline std::set<std::string> FamilyFiles(const lhvm::LHVMFile& program, std::string_view family)
{
	std::set<std::string> files;
	for (const auto& script : program.GetScripts())
	{
		const auto stem = std::filesystem::path(script.filename).stem().string();
		if (Folded(stem) == Folded(family))
		{
			files.insert(script.filename);
		}
	}
	return files;
}

/// The families the progress page lists as running whole, by its names
constexpr std::array<std::string_view, 48> k_RunningFamilies = {
    "Standard Reminder",
    "Vortex Entry",
    "Challenge Notify",
    "Did You Know",
    "Help System",
    "Reward",
    "Citadel Help",
    "Creature Help",
    "Hand Demos",
    "Setup Computer Creatures",
    "Citadel Guide",
    "Protect Gate Keys",
    "The Sculptor",
    "Creature Saving People",
    "Leave Through Vortex L1",
    "Singing Stone Circle",
    "Take Over Villages L1",
    "Throwing Stones",
    "Magic Mushroom",
    "Setup Land 2",
    "Slavers Warning",
    "Sacrifice",
    "Baywatch",
    "Land 2Shield Challenge",
    "Land 2Fireball Challenge",
    "Land 2Computer AI",
    "Begin Land 2",
    "Learn Worshipping",
    "Greedy Farmer",
    "Idol Pyre",
    "Leave Through Vortex L2",
    "Plague",
    "Hanoi Flood",
    "Tree Puzzle One",
    "Learn Gestures",
    "Learn Influence",
    "Final Gold Scroll",
    "Setup Land 3",
    "Leave Through Vortex L3",
    "Get Through Vortex L2",
    "Tree Puzzle Two",
    "Setup Land 4",
    "Leave Through Vortex L4",
    "Take Over Villages L4",
    "Fire On High",
    "Volcano",
    "Japanese Traitor",
    "Setup Land 5",
};

} // namespace openblack::test::challenge_scan
