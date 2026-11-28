/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "ModLoader.h"
#include "TestTempFolder.h"
#include "generated/ModSdk/ModSdkVersion.h"

namespace
{
/// A mods folder of its own in the temporary directory, removed with the test
class ModLoaderTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		const auto* test = ::testing::UnitTest::GetInstance()->current_test_info();
		_folder = openblack::test::UniqueTempPath(std::string("openblack_mod_loader_") + test->name());
		std::filesystem::remove_all(_folder);
		std::filesystem::create_directories(_folder);
	}
	void TearDown() override { std::filesystem::remove_all(_folder); }

	void WriteVersion(std::string_view text) const
	{
		std::ofstream file(_folder / "version.txt", std::ios::binary);
		file << text;
	}

	/// A mod's folder with its mod.txt
	void WriteMod(std::string_view folder, std::string_view manifest) const
	{
		std::filesystem::create_directories(_folder / folder);
		std::ofstream file(_folder / folder / "mod.txt", std::ios::binary);
		file << manifest;
	}

	/// ListMods on the folder: each mod as "folder|name|version|author|enabled"
	int List(std::vector<std::string>& mods) const
	{
		const auto keep = [](const ModLoaderModInfo* mod, void* context) {
			static_cast<std::vector<std::string>*>(context)->emplace_back(std::string(mod->folder) + "|" + mod->name + "|" +
			                                                              mod->version + "|" + mod->author + "|" +
			                                                              std::to_string(mod->enabled));
		};
		return ModLoader_ListMods(_folder.string().c_str(), keep, &mods);
	}

	/// LoadMods on the folder, keeping every line it reports
	int Load()
	{
		const auto keep = [](const char* line, void* context) {
			static_cast<std::vector<std::string>*>(context)->emplace_back(line);
		};
		return ModLoader_LoadMods(_folder.string().c_str(), keep, &_lines);
	}

	std::filesystem::path _folder;
	std::vector<std::string> _lines;
};
} // namespace

TEST(ModLoader, VersionIsTheHeadersVersion)
{
	EXPECT_EQ(ModLoader_Version(), k_ModLoaderVersion);
}

TEST_F(ModLoaderTest, NoVersionFileReadsNoMods)
{
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::NoVersionFile));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_EQ(_lines[0], "version.txt missing: no mods read");
}

TEST_F(ModLoaderTest, WrongVersionReadsNoMods)
{
	WriteVersion(std::to_string(k_ModLoaderVersion + 1));
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::VersionMismatch));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_NE(_lines[0].find("no mods read"), std::string::npos);
}

TEST_F(ModLoaderTest, NotANumberReadsNoMods)
{
	WriteVersion("1 beta");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::UnreadableVersion));
	ASSERT_EQ(_lines.size(), 1u);
	WriteVersion("");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::UnreadableVersion));
	EXPECT_EQ(_lines.size(), 2u);
}

TEST_F(ModLoaderTest, RightVersionMatches)
{
	WriteVersion(" " + std::to_string(k_ModLoaderVersion) + "\r\n");
	EXPECT_EQ(Load(), static_cast<int>(ModLoaderResult::Matched));
	ASSERT_EQ(_lines.size(), 1u);
	EXPECT_EQ(_lines[0], "version.txt matches version " + std::to_string(k_ModLoaderVersion));
}

TEST(ModLoader, NoFolderOrNoLogIsSafe)
{
	EXPECT_EQ(ModLoader_LoadMods(nullptr, nullptr, nullptr), static_cast<int>(ModLoaderResult::NoVersionFile));
}

TEST_F(ModLoaderTest, TheModsAreTheFoldersWithAManifestByName)
{
	WriteVersion(std::to_string(k_ModLoaderVersion));
	WriteMod("zeta", "format: 1\nname: Zeta\nversion: 2.0\nauthor: Z\n");
	WriteMod("alpha", "format: 1\nname: Alpha\n");
	WriteMod("broken", "name: no format\n");
	std::filesystem::create_directories(_folder / "empty");
	std::vector<std::string> mods;
	EXPECT_EQ(List(mods), 2);
	EXPECT_EQ(mods, (std::vector<std::string> {"alpha|Alpha|||0", "zeta|Zeta|2.0|Z|0"}));
}

TEST_F(ModLoaderTest, AnUnreadFolderListsNoMods)
{
	WriteMod("alpha", "format: 1\nname: Alpha\n");
	std::vector<std::string> mods;
	EXPECT_EQ(List(mods), -1);
	WriteVersion(std::to_string(k_ModLoaderVersion + 1));
	EXPECT_EQ(List(mods), -1);
	EXPECT_TRUE(mods.empty());
	EXPECT_EQ(ModLoader_ListMods(nullptr, nullptr, nullptr), -1);
}

TEST_F(ModLoaderTest, ASwitchedModIsKeptInTheEnabledList)
{
	WriteVersion(std::to_string(k_ModLoaderVersion));
	WriteMod("alpha", "format: 1\nname: Alpha\n");
	WriteMod("beta", "format: 1\nname: Beta\n");
	const auto folder = _folder.string();
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "beta", 1), static_cast<int>(ModLoaderSetResult::Kept));
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "alpha", 1), static_cast<int>(ModLoaderSetResult::Kept));
	std::vector<std::string> mods;
	EXPECT_EQ(List(mods), 2);
	EXPECT_EQ(mods, (std::vector<std::string> {"alpha|Alpha|||1", "beta|Beta|||1"}));
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "beta", 0), static_cast<int>(ModLoaderSetResult::Kept));
	std::ifstream file(_folder / "enabled.txt", std::ios::binary);
	const std::string text {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
	EXPECT_EQ(text, "alpha\n");
}

TEST_F(ModLoaderTest, OnlyAModOfAReadFolderCanBeSwitched)
{
	WriteMod("alpha", "format: 1\nname: Alpha\n");
	const auto folder = _folder.string();
	const auto noSuchMod = static_cast<int>(ModLoaderSetResult::NoSuchMod);
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "alpha", 1), noSuchMod);
	WriteVersion(std::to_string(k_ModLoaderVersion));
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "missing", 1), noSuchMod);
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), "../alpha", 1), noSuchMod);
	EXPECT_EQ(ModLoader_SetModEnabled(folder.c_str(), nullptr, 1), noSuchMod);
	EXPECT_FALSE(std::filesystem::exists(_folder / "enabled.txt"));
}

TEST(ModLoader, TheLibraryCarriesTheSdkVersionItWasBuiltWith)
{
	// the hash of the SDK-facing headers, generated at build time into the game and the library alike
	const std::string_view version = ModLoader_SdkVersion();
	EXPECT_EQ(version, std::string_view(k_ModSdkVersion));
	EXPECT_EQ(version.size(), 64u);
}

TEST_F(ModLoaderTest, TheEnabledModsFoldersAreGivenInTheListsOrder)
{
	WriteMod("beta", "format: 1\nname: Beta\n");
	WriteMod("alpha", "format: 1\nname: Alpha\n");
	WriteMod("gamma", "format: 1\nname: Gamma\n");
	const auto folder = _folder.string();
	const auto keep = [](const char* name, void* context) {
		static_cast<std::vector<std::string>*>(context)->emplace_back(name);
	};
	std::vector<std::string> folders;
	// an unread mods folder gives none
	EXPECT_EQ(ModLoader_EnabledModFolders(folder.c_str(), keep, &folders), -1);
	WriteVersion(std::to_string(k_ModLoaderVersion));
	EXPECT_EQ(ModLoader_EnabledModFolders(folder.c_str(), keep, &folders), 0);
	ModLoader_SetModEnabled(folder.c_str(), "gamma", 1);
	ModLoader_SetModEnabled(folder.c_str(), "alpha", 1);
	// a folder listed but gone is skipped
	{
		std::ofstream file(_folder / "enabled.txt", std::ios::binary | std::ios::app);
		file << "gone\n";
	}
	EXPECT_EQ(ModLoader_EnabledModFolders(folder.c_str(), keep, &folders), 2);
	EXPECT_EQ(folders, (std::vector<std::string> {"alpha", "gamma"}));
	EXPECT_EQ(ModLoader_EnabledModFolders(folder.c_str(), nullptr, nullptr), 2);
}
