/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <set>
#include <string>

#include <gtest/gtest.h>

#include "ModManifest.h"

using namespace mod_loader;

TEST(ModManifest, AManifestNamesItsModVersionAndAuthor)
{
	const auto manifest = ParseManifest("# a sample mod\r\nformat: 1\r\nname:  Greener Grass \r\nversion: 1.2\r\n"
	                                    "author: Someone\r\nhomepage: ignored for now\r\n");
	ASSERT_TRUE(manifest.has_value());
	EXPECT_EQ(manifest->name, "Greener Grass");
	EXPECT_EQ(manifest->version, "1.2");
	EXPECT_EQ(manifest->author, "Someone");
}

TEST(ModManifest, VersionAndAuthorMayBeMissingAndTheFirstValueOfAKeyStays)
{
	const auto manifest = ParseManifest("format: 1\nname: First\nname: Second\n");
	ASSERT_TRUE(manifest.has_value());
	EXPECT_EQ(manifest->name, "First");
	EXPECT_TRUE(manifest->version.empty());
	EXPECT_TRUE(manifest->author.empty());
}

TEST(ModManifest, WithoutTheFormatFirstOrANameItIsNoMod)
{
	EXPECT_FALSE(ParseManifest("").has_value());
	EXPECT_FALSE(ParseManifest("name: No format\n").has_value());
	EXPECT_FALSE(ParseManifest("name: Format second\nformat: 1\n").has_value());
	EXPECT_FALSE(ParseManifest("format: 2\nname: A later format\n").has_value());
	EXPECT_FALSE(ParseManifest("format: 1 beta\nname: Not a number\n").has_value());
	EXPECT_FALSE(ParseManifest("format: 1\nversion: 1.0\n").has_value());
	EXPECT_FALSE(ParseManifest("format: 1\nname:\n").has_value());
	EXPECT_FALSE(ParseManifest("not a key\nformat: 1\nname: Junk first\n").has_value());
}

TEST(ModManifest, TheEnabledListIsOneFolderALineAndWritesBackTheSame)
{
	const auto enabled = ParseEnabled("# switched on\r\nzeta\r\n\r\n alpha \r\n../outside\r\na/b\r\n.\r\n");
	EXPECT_EQ(enabled, (std::set<std::string> {"alpha", "zeta"}));
	EXPECT_EQ(WriteEnabled(enabled), "alpha\nzeta\n");
	EXPECT_EQ(ParseEnabled(WriteEnabled(enabled)), enabled);
	EXPECT_TRUE(ParseEnabled("").empty());
}

TEST(ModManifest, AModFolderNameIsOneFolderOfTheModsFolder)
{
	EXPECT_TRUE(IsModFolderName("GreenerGrass"));
	EXPECT_FALSE(IsModFolderName(""));
	EXPECT_FALSE(IsModFolderName("."));
	EXPECT_FALSE(IsModFolderName(".."));
	EXPECT_FALSE(IsModFolderName("a/b"));
	EXPECT_FALSE(IsModFolderName("a\\b"));
}
