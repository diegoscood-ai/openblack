/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// F2's question about going to the Gods' Playground (input::playground_key): when the key asks it, and what each answer
// does, in its order. docs/wiki/project/progress/story/tutorial_island.md

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Input/PlaygroundKey.h"

using namespace openblack::input::playground_key;

namespace
{
/// A story game on the first land, in play: the key asks
constexpr Gates k_InPlay {.land = 1};

/// Records each step of the trip as it is taken
class RecordedSteps final: public TripSteps
{
public:
	void Unpause() override { taken.emplace_back("unpause"); }
	void LeaveTemple() override { taken.emplace_back("leave temple"); }
	void Save() override { taken.emplace_back("save"); }
	void StopAllScripts() override { taken.emplace_back("stop scripts"); }
	void ChangeLand(std::string_view landScript) override { taken.push_back("land " + std::string(landScript)); }
	void LessonStarted() override { taken.emplace_back("lesson"); }
	void StartScript(std::string_view script) override { taken.push_back("start " + std::string(script)); }

	std::vector<std::string> taken;
};
} // namespace

TEST(PlaygroundKey, AsksInAStoryGameOnAnyLandButTheIsland)
{
	for (int32_t land = 1; land <= 5; ++land)
	{
		auto gates = k_InPlay;
		gates.land = land;
		EXPECT_TRUE(KeyAsks(gates)) << "land " << land;
	}
	auto island = k_InPlay;
	island.land = k_PlaygroundLand;
	EXPECT_FALSE(KeyAsks(island));
}

TEST(PlaygroundKey, EachGateStopsIt)
{
	auto skirmish = k_InPlay;
	skirmish.skirmish = true;
	EXPECT_FALSE(KeyAsks(skirmish));
	auto network = k_InPlay;
	network.networkGame = true;
	EXPECT_FALSE(KeyAsks(network));
	auto temple = k_InPlay;
	temple.insideTemple = true;
	EXPECT_FALSE(KeyAsks(temple));
	auto box = k_InPlay;
	box.boxUp = true;
	EXPECT_FALSE(KeyAsks(box));
	auto wideScreen = k_InPlay;
	wideScreen.scriptWideScreen = true;
	EXPECT_FALSE(KeyAsks(wideScreen));
}

TEST(PlaygroundKey, YesSavesStopsTheScriptsLoadsTheIslandAndStartsItsScript)
{
	RecordedSteps steps;
	Answer(true, steps);
	const std::vector<std::string> expected {
	    "unpause", "leave temple", "save", "stop scripts", "land LandT.txt", "lesson", "start LandControlT",
	};
	EXPECT_EQ(steps.taken, expected);
}

TEST(PlaygroundKey, NoOnlyLetsTheGameGoOn)
{
	RecordedSteps steps;
	Answer(false, steps);
	EXPECT_EQ(steps.taken, std::vector<std::string> {"unpause"});
}

TEST(PlaygroundKey, TheQuestionIsItsHelpText)
{
	// "Are you sure you want to go to the Gods' Playground?", the help text of that number
	EXPECT_EQ(k_QuestionText, 6653U);
}
