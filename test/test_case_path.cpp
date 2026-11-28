/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "FileSystem/CasePath.h"

using namespace openblack::filesystem::case_path;

namespace
{
/// A case-sensitive file system: the paths that exist, and each folder's names in the order it lists them
struct FakeDisk
{
	std::set<std::string> paths;
	std::map<std::string, std::vector<std::string>> folders;

	[[nodiscard]] auto Exists() const
	{
		return [this](const std::filesystem::path& path) { return paths.contains(path.generic_string()); };
	}
	[[nodiscard]] auto NamesIn() const
	{
		return [this](const std::filesystem::path& folder) {
			const auto it = folders.find(folder.generic_string());
			return it != folders.end() ? it->second : std::vector<std::string> {};
		};
	}
};

FakeDisk GameFolder()
{
	return FakeDisk {
	    .paths = {"/game", "/game/Data", "/game/Data/landscape", "/game/Data/landscape/Land1.lnd", "/game/Scripts",
	              "/game/Scripts/Challenge.chl"},
	    .folders = {{"/", {"game"}},
	                {"/game", {"Data", "Scripts"}},
	                {"/game/Data", {"landscape"}},
	                {"/game/Data/landscape", {"Land1.lnd"}},
	                {"/game/Scripts", {"Challenge.chl"}}},
	};
}
} // namespace

TEST(CasePath, NamesAreTheSameWhateverTheCaseOfTheirLetters)
{
	EXPECT_TRUE(SameNameIgnoringCase("Land1.LND", "land1.lnd"));
	EXPECT_TRUE(SameNameIgnoringCase("", ""));
	EXPECT_FALSE(SameNameIgnoringCase("Land1.lnd", "Land2.lnd"));
	EXPECT_FALSE(SameNameIgnoringCase("Land1", "Land1.lnd"));
}

TEST(CasePath, APathAsWrittenIsItself)
{
	const auto disk = GameFolder();
	const auto found = FindIgnoringCase("/game/Data/landscape/Land1.lnd", disk.Exists(), disk.NamesIn());
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->generic_string(), "/game/Data/landscape/Land1.lnd");
}

TEST(CasePath, EachFolderAndTheFileAreFoundWhateverTheirCase)
{
	const auto disk = GameFolder();
	const auto found = FindIgnoringCase("/game/DATA/Landscape/LAND1.lnd", disk.Exists(), disk.NamesIn());
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->generic_string(), "/game/Data/landscape/Land1.lnd");
	// a leading "." is kept as it is
	const FakeDisk relative {.paths = {"./Scripts", "./Scripts/Challenge.chl"},
	                         .folders = {{".", {"Scripts"}}, {"./Scripts", {"Challenge.chl"}}}};
	const auto inCurrent = FindIgnoringCase("./scripts/challenge.CHL", relative.Exists(), relative.NamesIn());
	ASSERT_TRUE(inCurrent.has_value());
	EXPECT_EQ(inCurrent->generic_string(), "./Scripts/Challenge.chl");
}

TEST(CasePath, APartThatIsNotThereInAnyCaseFindsNothing)
{
	const auto disk = GameFolder();
	EXPECT_FALSE(FindIgnoringCase("/game/Data/landscape/Land2.lnd", disk.Exists(), disk.NamesIn()).has_value());
	EXPECT_FALSE(FindIgnoringCase("/game/Audio/Land1.lnd", disk.Exists(), disk.NamesIn()).has_value());
	EXPECT_FALSE(FindIgnoringCase("", disk.Exists(), disk.NamesIn()).has_value());
}

TEST(CasePath, OfTwoNamesTheSameButForCaseTheFirstTheFolderListsIsTaken)
{
	FakeDisk disk {.paths = {"/game", "/game/a.txt", "/game/A.TXT"},
	               .folders = {{"/", {"game"}}, {"/game", {"A.TXT", "a.txt"}}}};
	const auto found = FindIgnoringCase("/game/A.txt", disk.Exists(), disk.NamesIn());
	ASSERT_TRUE(found.has_value());
	EXPECT_EQ(found->generic_string(), "/game/A.TXT");
}
