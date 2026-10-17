/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug bar's "Load Island": without the creature it only loads the island (LoadMap). With it, it goes the game's
// land change way (ChangeLand, which saves the creature first), and does what the game does around it: a story land's
// scripts stopped and its control script started, a playground's scripts stopped and the profile's creature made at the
// temple, fully grown. A fake game records the calls in their order.

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include "Debug/IslandLoad.h"

using openblack::debug::gui::ClassifyIslandScript;
using openblack::debug::gui::IslandKind;
using openblack::debug::gui::LoadIsland;

namespace
{
struct Point
{
	float x;
	float z;
};

struct FakeGame
{
	std::vector<std::string> calls;
	bool result = true;
	std::optional<Point> templeHome = Point {1.5f, 2.5f};
	bool creatureMade = true;

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
	void StopAllTasks() { calls.emplace_back("StopAllTasks"); }
	void StartScript(const std::string& name) { calls.push_back("StartScript " + name); }
	std::optional<Point> TempleCreatureHome() { return templeHome; }
	std::optional<int> LoadMyCreature(Point home)
	{
		calls.push_back(fmt::format("LoadMyCreature {} {}", home.x, home.z));
		return creatureMade ? std::optional<int>(7) : std::nullopt;
	}
	void SetCreatureStage(int creature, int stage) { calls.push_back(fmt::format("SetCreatureStage {} {}", creature, stage)); }
	void Log(const std::string& /*line*/) { calls.emplace_back("Log"); }
};

using Calls = std::vector<std::string>;
} // namespace

TEST(IslandLoad, StoryLandsAreKnownByTheirScriptsNameInAnyFolderAndCase)
{
	for (int land = 1; land <= 5; ++land)
	{
		const auto island = ClassifyIslandScript(fmt::format("Scripts/Land{}.txt", land));
		EXPECT_EQ(island.kind, IslandKind::StoryLand);
		EXPECT_EQ(island.land, land);
	}
	EXPECT_EQ(ClassifyIslandScript("scripts/LAND3.TXT").land, 3);
	EXPECT_EQ(ClassifyIslandScript("Other/Folder/land2.txt").land, 2);
	EXPECT_EQ(ClassifyIslandScript("land4.txt").land, 4);
}

TEST(IslandLoad, PlaygroundsAreTheScriptsInAPlaygroundsFolder)
{
	EXPECT_EQ(ClassifyIslandScript("Scripts/Playgrounds/TwoGods.txt").kind, IslandKind::Playground);
	EXPECT_EQ(ClassifyIslandScript("scripts/playgrounds/Land2.txt").kind, IslandKind::Playground);
	EXPECT_EQ(ClassifyIslandScript("scripts/playgrounds/Land2.txt").land, 0);
}

TEST(IslandLoad, AnythingElseIsOther)
{
	for (const auto* path : {"Scripts/Land0.txt", "Scripts/Land6.txt", "Scripts/LandT.txt", "Scripts/Land12.txt",
	                         "Scripts/Land2.chl", "Scripts/MyLand2.txt", "Scripts/Playgrounds.txt", ""})
	{
		EXPECT_EQ(ClassifyIslandScript(path).kind, IslandKind::Other) << path;
		EXPECT_EQ(ClassifyIslandScript(path).land, 0) << path;
	}
}

TEST(IslandLoad, ALaterStoryLandStopsTheScriptsChangesLandAndStartsItsControlScript)
{
	FakeGame game;
	EXPECT_TRUE(LoadIsland(game, "Scripts/Land2.txt", true));
	EXPECT_EQ(game.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Land2.txt", "StartScript LandControl2"}));

	FakeGame game5;
	EXPECT_TRUE(LoadIsland(game5, "Scripts/land5.txt", true));
	EXPECT_EQ(game5.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/land5.txt", "StartScript LandControl5"}));
}

TEST(IslandLoad, LandOneStartsTheWholeStorysControlScriptAsANewGame)
{
	FakeGame game;
	EXPECT_TRUE(LoadIsland(game, "Scripts/Land1.txt", true));
	EXPECT_EQ(game.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Land1.txt", "StartScript LandControlAll"}));
}

TEST(IslandLoad, APlaygroundMakesTheProfilesCreatureAtTheTempleFullyGrown)
{
	FakeGame game;
	EXPECT_TRUE(LoadIsland(game, "Scripts/Playgrounds/TwoGods.txt", true));
	EXPECT_EQ(game.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Playgrounds/TwoGods.txt", "LoadMyCreature 1.5 2.5",
	                              "SetCreatureStage 7 13"}));
}

TEST(IslandLoad, APlaygroundWithoutATempleMakesNoCreatureAndSaysSo)
{
	FakeGame game;
	game.templeHome.reset();
	EXPECT_TRUE(LoadIsland(game, "Scripts/Playgrounds/TwoGods.txt", true));
	EXPECT_EQ(game.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Playgrounds/TwoGods.txt", "Log"}));
}

TEST(IslandLoad, APlaygroundCreatureThatIsNotMadeIsNotGrown)
{
	FakeGame game;
	game.creatureMade = false;
	EXPECT_TRUE(LoadIsland(game, "Scripts/Playgrounds/TwoGods.txt", true));
	EXPECT_EQ(game.calls,
	          (Calls {"StopAllTasks", "ChangeLand Scripts/Playgrounds/TwoGods.txt", "LoadMyCreature 1.5 2.5", "Log"}));
}

TEST(IslandLoad, AnyOtherScriptOnlyChangesLand)
{
	FakeGame game;
	EXPECT_TRUE(LoadIsland(game, "Scripts/LandT.txt", true));
	EXPECT_EQ(game.calls, (Calls {"ChangeLand Scripts/LandT.txt"}));
}

TEST(IslandLoad, WithoutTheCreatureOnlyTheIslandIsLoaded)
{
	for (const auto* path : {"Scripts/Land2.txt", "Scripts/Land1.txt", "Scripts/Playgrounds/TwoGods.txt"})
	{
		FakeGame game;
		EXPECT_TRUE(LoadIsland(game, path, false));
		EXPECT_EQ(game.calls, (Calls {std::string("LoadMap ") + path}));
	}
}

TEST(IslandLoad, AFailedLandChangeStartsNothingMore)
{
	FakeGame story;
	story.result = false;
	EXPECT_FALSE(LoadIsland(story, "Scripts/Land3.txt", true));
	EXPECT_EQ(story.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Land3.txt", "Log"}));

	FakeGame playground;
	playground.result = false;
	EXPECT_FALSE(LoadIsland(playground, "Scripts/Playgrounds/TwoGods.txt", true));
	EXPECT_EQ(playground.calls, (Calls {"StopAllTasks", "ChangeLand Scripts/Playgrounds/TwoGods.txt", "Log"}));
}

TEST(IslandLoad, ItAnswersAsTheLoadDid)
{
	FakeGame game;
	game.result = false;
	EXPECT_FALSE(LoadIsland(game, "Scripts/LandT.txt", true));
	EXPECT_FALSE(LoadIsland(game, "Scripts/Land3.txt", false));
}
