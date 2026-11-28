/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdlib>
#include <cstring>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <vector>

#include "InfoConstants.h"

// The original game's data for the integration tests: its folder, from OPENBLACK_GAME_PATH, and its info.dat
namespace openblack::test
{
/// The game's folder, from OPENBLACK_GAME_PATH
[[nodiscard]] inline std::optional<std::filesystem::path> GamePath()
{
	const char* game = std::getenv("OPENBLACK_GAME_PATH");
	if (game == nullptr)
	{
		return std::nullopt;
	}
	return std::filesystem::path(game);
}

/// info.dat's table as the game reads it (after the file's header), or nothing when the file is missing or of another
/// size
[[nodiscard]] inline std::unique_ptr<InfoConstants> ReadInfoDat(const std::filesystem::path& game)
{
	std::ifstream file(game / "Scripts" / "info.dat", std::ios::binary);
	if (!file.is_open())
	{
		return nullptr;
	}
	const std::vector<char> data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	constexpr size_t k_Header = 0x2C;
	if (data.size() != k_Header + sizeof(InfoConstants))
	{
		return nullptr;
	}
	auto info = std::make_unique<InfoConstants>();
	std::memcpy(info.get(), data.data() + k_Header, sizeof(InfoConstants));
	return info;
}

} // namespace openblack::test
