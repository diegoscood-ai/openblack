/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <string_view>

namespace openblack::mod_sdk
{

/// How the game and the mod loader library agree at start-up. The SDK never holds the game's code back: a change to
/// the SDK-facing headers (src/ModSdk) moves the SDK's version, and a library not rebuilt with it is not used, so the
/// game starts unmodded with one log line saying why
enum class Handshake
{
	/// No library next to the game
	NoLibrary,
	/// A library without the functions this SDK calls
	MissingFunctions,
	/// A library built from other SDK headers than the game's
	VersionMismatch,
	/// The same SDK: the mods folder is read
	Active,
};

/// The handshake from what the game found: whether the library loaded, whether it has every function, the SDK version
/// it reports and the game's own
[[nodiscard]] constexpr Handshake Check(bool libraryLoaded, bool functionsFound, std::string_view librarySdk,
                                        std::string_view gameSdk)
{
	if (!libraryLoaded)
	{
		return Handshake::NoLibrary;
	}
	if (!functionsFound)
	{
		return Handshake::MissingFunctions;
	}
	return librarySdk == gameSdk ? Handshake::Active : Handshake::VersionMismatch;
}

/// The log line for a handshake that leaves the game unmodded; empty when the SDK is active
[[nodiscard]] constexpr std::string_view WhyUnmodded(Handshake handshake)
{
	switch (handshake)
	{
	case Handshake::NoLibrary:
		return "ModLoader: no library, no mods";
	case Handshake::MissingFunctions:
		return "ModLoader: the library lacks this SDK's functions, no mods";
	case Handshake::VersionMismatch:
		return "ModLoader: the library was built from another SDK version than the game's, no mods (rebuild it with "
		       "the game)";
	case Handshake::Active:
		break;
	}
	return {};
}

} // namespace openblack::mod_sdk
