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
#include <array>
#include <optional>
#include <string_view>
#include <utility>

#include "EngineConfig.h"
#include "Windowing/WindowingInterface.h"

// What the command line's names choose: the graphics interface and the window's mode. Pure: values only.
namespace openblack::launch_options
{

/// The graphics interface drawn through when the command line names none
[[nodiscard]] constexpr GraphicsBackend DefaultBackend()
{
#ifdef _APPLE_
	return GraphicsBackend::Metal;
#else
	return GraphicsBackend::Vulkan;
#endif
}

/// The graphics interface of a name the command line accepts (k_GraphicsBackendStringLookup), none for another
[[nodiscard]] constexpr std::optional<GraphicsBackend> BackendNamed(std::string_view name)
{
	const auto found =
	    std::ranges::find(k_GraphicsBackendStringLookup, name, &std::pair<std::string_view, GraphicsBackend>::first);
	return found != k_GraphicsBackendStringLookup.cend() ? std::optional(found->second) : std::nullopt;
}

/// The window modes by their command line names
inline constexpr std::array<std::pair<std::string_view, windowing::DisplayMode>, 3> k_DisplayModes {{
    {"windowed", windowing::DisplayMode::Windowed},
    {"fullscreen", windowing::DisplayMode::Fullscreen},
    {"borderless", windowing::DisplayMode::Borderless},
}};

/// The window mode of a name, none for another
[[nodiscard]] constexpr std::optional<windowing::DisplayMode> DisplayModeNamed(std::string_view name)
{
	const auto found = std::ranges::find(k_DisplayModes, name, &std::pair<std::string_view, windowing::DisplayMode>::first);
	return found != k_DisplayModes.cend() ? std::optional(found->second) : std::nullopt;
}

/// The pre-intro film's word on the command line, as the game's own switches are given
inline constexpr std::string_view k_PreIntroVideoSwitch = "PREINTROVIDEO";

/// Whether the command line gives one of the game's own switch words: anywhere a '-' or a '/' is followed by the word,
/// its letters in any case; what follows the word does not matter
[[nodiscard]] constexpr bool HasGameSwitch(std::string_view commandLine, std::string_view word)
{
	const auto lower = [](char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; };
	for (size_t at = 0; at < commandLine.size(); ++at)
	{
		if (commandLine[at] != '-' && commandLine[at] != '/')
		{
			continue;
		}
		const auto rest = commandLine.substr(at + 1);
		if (rest.size() >= word.size() && std::ranges::equal(rest.substr(0, word.size()), word, {}, lower, lower))
		{
			return true;
		}
	}
	return false;
}

/// The music's word: none of the game's music is loaded, so no music plays
inline constexpr std::string_view k_NoLoadMusicSwitch = "NOLOADMUSIC";
/// The settings file's word, the file's name right after it
inline constexpr std::string_view k_SettingsSwitch = "SETTINGS:";

/// The settings file the command line names: what follows the last SETTINGS: switch, up to the next space or the
/// line's end. Empty when none is named
[[nodiscard]] constexpr std::string_view SettingsFileNamed(std::string_view commandLine)
{
	std::string_view name;
	for (size_t at = 0; at < commandLine.size(); ++at)
	{
		if ((commandLine[at] == '-' || commandLine[at] == '/') &&
		    HasGameSwitch(commandLine.substr(at, k_SettingsSwitch.size() + 1), k_SettingsSwitch))
		{
			const auto rest = commandLine.substr(at + 1 + k_SettingsSwitch.size());
			name = rest.substr(0, rest.find(' '));
		}
	}
	return name;
}

/// Whether one argument is a game switch word, which the named options do not take: it starts with '-' or '/' and the
/// word follows, in any case
[[nodiscard]] constexpr bool IsGameSwitchArgument(std::string_view argument, std::string_view word)
{
	return !argument.empty() && (argument.front() == '-' || argument.front() == '/') &&
	       HasGameSwitch(argument.substr(0, word.size() + 1), word);
}

} // namespace openblack::launch_options
