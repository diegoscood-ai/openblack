/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FileSystem/ModSearchRoots.h"

using namespace openblack::filesystem::mod_roots;

namespace
{
/// Folders that hold the files named, by their generic string
struct FakeFolders
{
	std::set<std::string> files;
	bool operator()(const std::filesystem::path& path) const { return files.contains(path.generic_string()); }
};
} // namespace

TEST(ModSearchRoots, AModsFileIsFoundBelowItsFolder)
{
	const std::vector<std::filesystem::path> roots {"Mods/hd"};
	const FakeFolders folders {{"Mods/hd/Data/AllMeshes.g3d"}};
	const auto found = FirstInRoots(roots, "Data/AllMeshes.g3d", folders);
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->generic_string(), "Mods/hd/Data/AllMeshes.g3d");
}

TEST(ModSearchRoots, AFileNoModHoldsFallsBackToTheGame)
{
	const std::vector<std::filesystem::path> roots {"Mods/hd"};
	const FakeFolders folders {{"Mods/hd/Data/AllMeshes.g3d"}};
	EXPECT_FALSE(FirstInRoots(roots, "Data/Textures/Sky.raw", folders).has_value());
}

TEST(ModSearchRoots, TheFirstModInTheOrderWins)
{
	const std::vector<std::filesystem::path> roots {"Mods/alpha", "Mods/beta"};
	const FakeFolders folders {{"Mods/alpha/Data/a.txt", "Mods/beta/Data/a.txt", "Mods/beta/Data/b.txt"}};
	EXPECT_EQ(FirstInRoots(roots, "Data/a.txt", folders)->generic_string(), "Mods/alpha/Data/a.txt");
	EXPECT_EQ(FirstInRoots(roots, "Data/b.txt", folders)->generic_string(), "Mods/beta/Data/b.txt");
}

TEST(ModSearchRoots, NoRootsReplaceNothing)
{
	// without the mod loader, or with no mod switched on, the file system has no roots
	const FakeFolders folders {{"Mods/hd/Data/AllMeshes.g3d"}};
	EXPECT_FALSE(FirstInRoots({}, "Data/AllMeshes.g3d", folders).has_value());
}

TEST(ModSearchRoots, OnlyPathsOfTheGamesFolderAreReplaced)
{
	// an absolute folder on any system
	const auto base = std::filesystem::temp_directory_path();
	const auto game = base / "BW";
	EXPECT_EQ(GameRelative("Data/AllMeshes.g3d", game)->generic_string(), "Data/AllMeshes.g3d");
	EXPECT_EQ(GameRelative(game / "Data" / "AllMeshes.g3d", game)->generic_string(), "Data/AllMeshes.g3d");
	EXPECT_FALSE(GameRelative(base / "Elsewhere" / "x.txt", game).has_value());
	EXPECT_FALSE(GameRelative(base / "BW2" / "x.txt", game).has_value());
	EXPECT_FALSE(GameRelative({}, game).has_value());
	EXPECT_FALSE(GameRelative(game / "x.txt", {}).has_value());
}
