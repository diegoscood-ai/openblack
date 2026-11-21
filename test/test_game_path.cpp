/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The game's install folder comes from the command line, which wins, or from the registry's GameDir, which is only read
// without one; with neither the game cannot start. docs/wiki/project/progress/engine/file_system_and_packs.md

#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "GamePath.h"

using namespace openblack;

TEST(GamePath, TheCommandLineWinsAndTheRegistryIsNotRead)
{
	int read = 0;
	const auto registry = [&read]() -> std::optional<std::string> {
		++read;
		return "C:/Lionhead/Black & White";
	};
	EXPECT_EQ(game_path::Choose("D:/bw", registry), "D:/bw");
	EXPECT_EQ(read, 0);
}

TEST(GamePath, WithoutTheCommandLineTheRegistrysGameDirIsTaken)
{
	const auto registry = []() -> std::optional<std::string> { return "C:/Lionhead/Black & White"; };
	EXPECT_EQ(game_path::Choose(std::nullopt, registry), "C:/Lionhead/Black & White");
}

TEST(GamePath, WithNeitherThereIsNoFolder)
{
	const auto none = []() -> std::optional<std::string> { return std::nullopt; };
	EXPECT_EQ(game_path::Choose(std::nullopt, none), std::nullopt);
	EXPECT_EQ(game_path::Choose(std::nullopt, {}), std::nullopt);
}
