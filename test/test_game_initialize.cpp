/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <Game.h>
#include <PackFile.h>
#include <gtest/gtest.h>

#include "InfoConstants.h"
#include "Locator.h"
#include "TestTempFolder.h"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(GameInitialize, initializeOnly)
{
	static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
	const openblack::test::TestTempFolder profileFolder;
	auto args = openblack::Arguments {
	    .graphicsBackend = openblack::GraphicsBackend::Noop,
	    .gamePath = mockGamePath.string(),
	    .numFramesToSimulate = 0,
	    .logFile = "stdout",
	    .startLevel = "Land1.txt",
	};
	std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::debug);
	args.profileFolder = profileFolder.Path();
	auto game = std::make_unique<openblack::Game>(std::move(args));
	ASSERT_TRUE(game->Initialize());
	game.reset();
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(GameInitialize, run0Frames)
{
	static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
	const openblack::test::TestTempFolder profileFolder;
	auto args = openblack::Arguments {
	    .graphicsBackend = openblack::GraphicsBackend::Noop,
	    .gamePath = mockGamePath.string(),
	    .numFramesToSimulate = 0,
	    .logFile = "stdout",
	    .startLevel = "Land1.txt",
	};
	std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::debug);
	args.profileFolder = profileFolder.Path();
	auto game = std::make_unique<openblack::Game>(std::move(args));
	ASSERT_TRUE(game->Initialize());
	ASSERT_TRUE(game->Run());
	game.reset();
}

// The game reads every table of objects, buildings, miracles, creatures, villagers and dances from the Info block of
// Scripts/info.dat when it starts, as the original (docs/wiki/project/progress/scripts/info_scripts.md). On the mock
// game's file, whose tables name the first abode, the tables the game holds are the file's own, byte for byte
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(GameInitialize, theInfoTablesAreReadFromTheScriptsInfoFile)
{
	static const auto mockGamePath = std::filesystem::path(TEST_BINARY_DIR) / "mock";
	openblack::pack::PackFile pack;
	ASSERT_EQ(pack.Open(mockGamePath / "Scripts" / "info.dat"), openblack::pack::PackResult::Success);
	const auto& block = pack.GetBlock("Info");
	ASSERT_EQ(block.size(), sizeof(openblack::InfoConstants));

	const openblack::test::TestTempFolder profileFolder;
	auto args = openblack::Arguments {
	    .graphicsBackend = openblack::GraphicsBackend::Noop,
	    .gamePath = mockGamePath.string(),
	    .numFramesToSimulate = 0,
	    .logFile = "stdout",
	    .startLevel = "Land1.txt",
	};
	std::fill_n(args.logLevels.begin(), args.logLevels.size(), spdlog::level::debug);
	args.profileFolder = profileFolder.Path();
	auto game = std::make_unique<openblack::Game>(std::move(args));
	ASSERT_TRUE(game->Initialize());
	ASSERT_TRUE(openblack::Locator::infoConstants::has_value());
	const auto& tables = openblack::Locator::infoConstants::value();
	EXPECT_EQ(tables.abode.at(0).debugString.at(0), 'A');
	EXPECT_EQ(std::memcmp(&tables, block.data(), block.size()), 0);
	game.reset();
}
