/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug bar's "Load Island": with the creature taken along it goes the game's land change way (ChangeLand, which
// saves the creature first), else it only loads the island (LoadMap). A fake game records which was called.

#include <filesystem>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/IslandLoad.h"

namespace
{
struct FakeGame
{
	std::vector<std::string> calls;
	bool result = true;

	bool LoadMap(const std::filesystem::path& path)
	{
		calls.push_back("LoadMap " + path.generic_string());
		return result;
	}
	bool ChangeLand(const std::filesystem::path& path)
	{
		calls.push_back("ChangeLand " + path.generic_string());
		return result;
	}
};
} // namespace

TEST(IslandLoad, TakingTheCreatureAlongGoesTheGamesLandChangeWay)
{
	FakeGame game;
	EXPECT_TRUE(openblack::debug::gui::LoadIsland(game, "Scripts/Land2.txt", true));
	EXPECT_EQ(game.calls, (std::vector<std::string> {"ChangeLand Scripts/Land2.txt"}));
}

TEST(IslandLoad, WithoutTheCreatureOnlyTheIslandIsLoaded)
{
	FakeGame game;
	EXPECT_TRUE(openblack::debug::gui::LoadIsland(game, "Scripts/Land2.txt", false));
	EXPECT_EQ(game.calls, (std::vector<std::string> {"LoadMap Scripts/Land2.txt"}));
}

TEST(IslandLoad, ItAnswersAsTheLoadDid)
{
	FakeGame game;
	game.result = false;
	EXPECT_FALSE(openblack::debug::gui::LoadIsland(game, "Scripts/Land3.txt", true));
	EXPECT_FALSE(openblack::debug::gui::LoadIsland(game, "Scripts/Land3.txt", false));
}
