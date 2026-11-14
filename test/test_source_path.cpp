/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// string_utils::SourceRelativePath: the caller's file in the random trace, from inside the source tree

#include <string_view>

#include <gtest/gtest.h>

#include "Common/StringUtils.h"

using openblack::string_utils::SourceRelativePath;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, windowsPath)
{
	EXPECT_EQ(SourceRelativePath(R"(C:\Users\me\dev\openblack\src\Common\GameRandom.cpp)"), R"(Common\GameRandom.cpp)");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, posixPath)
{
	EXPECT_EQ(SourceRelativePath("/home/me/openblack/src/ECS/Registry.cpp"), "ECS/Registry.cpp");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, mixedSeparators)
{
	EXPECT_EQ(SourceRelativePath(R"(C:/tree\src/Game.cpp)"), "Game.cpp");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, lastSrcFolderWins)
{
	EXPECT_EQ(SourceRelativePath("/src/clone/src/Audio/src/Sound.cpp"), "Sound.cpp");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, onlyAWholeFolderName)
{
	// "src" inside a folder or file name is not the source folder
	EXPECT_EQ(SourceRelativePath("/home/mysrc/srcs/a/src.cpp"), "/home/mysrc/srcs/a/src.cpp");
	EXPECT_EQ(SourceRelativePath("/home/mysrc/src2/src/a.cpp"), "a.cpp");
}

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST(SourceRelativePath, noSourceFolder)
{
	EXPECT_EQ(SourceRelativePath("Game.cpp"), "Game.cpp");
	EXPECT_EQ(SourceRelativePath("src/Game.cpp"), "src/Game.cpp");
	EXPECT_EQ(SourceRelativePath("/a/src"), "/a/src");
	EXPECT_EQ(SourceRelativePath(""), "");
}

static_assert(SourceRelativePath("/x/src/y.cpp") == "y.cpp");
