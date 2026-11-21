/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The player's own files (FileSystemInterface::ReadUserFile): read as the game's files are, but a data mod's file of
// the same path never takes their place, and one that is not there reads as nothing.

#define LOCATOR_IMPLEMENTATIONS

#include <cstdint>

#include <filesystem>
#include <fstream>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "FileSystem/DefaultFileSystem.h"
#include "TestTempFolder.h"

using namespace openblack;

namespace
{
void Write(const std::filesystem::path& path, std::string_view text)
{
	std::filesystem::create_directories(path.parent_path());
	std::ofstream(path, std::ios::binary) << text;
}

std::vector<uint8_t> Bytes(std::string_view text)
{
	return {text.begin(), text.end()};
}
} // namespace

TEST(UserFiles, AModNeverTakesThePlayersFile)
{
	const test::TestTempFolder folder;
	const auto game = folder.Path() / "Game";
	const auto mod = folder.Path() / "Mod";
	Write(game / "SaveGame" / "vortex.txt", "the player's");
	Write(mod / "SaveGame" / "vortex.txt", "the mod's");
	filesystem::DefaultFileSystem files;
	files.SetGamePath(game);
	files.SetModRoots({mod});
	// a game file of that path is the mod's, the player's own file is not
	EXPECT_EQ(files.ReadAll(game / "SaveGame" / "vortex.txt"), Bytes("the mod's"));
	EXPECT_EQ(files.ReadUserFile(game / "SaveGame" / "vortex.txt"), Bytes("the player's"));
}

TEST(UserFiles, OneThatIsNotThereReadsAsNothing)
{
	const test::TestTempFolder folder;
	const auto game = folder.Path() / "Game";
	Write(folder.Path() / "Mod" / "SaveGame" / "vortex.txt", "the mod's");
	std::filesystem::create_directories(game);
	filesystem::DefaultFileSystem files;
	files.SetGamePath(game);
	files.SetModRoots({folder.Path() / "Mod"});
	EXPECT_FALSE(files.ReadUserFile(game / "SaveGame" / "vortex.txt").has_value());
	// without mods it reads the same as ReadAll does
	Write(game / "Profiles" / "setup.txt", "setup");
	files.SetModRoots({});
	EXPECT_EQ(files.ReadUserFile(game / "Profiles" / "setup.txt"), files.ReadAll(game / "Profiles" / "setup.txt"));
}
