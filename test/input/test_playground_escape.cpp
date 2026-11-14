/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Escape on the Gods' Playground (input::playground_escape): when the island takes the key, what it does, and the
// restart of the island's scripts on its free play, in its order. docs/wiki/project/progress/story/tutorial_island.md

#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Input/PlaygroundEscape.h"

using namespace openblack::input::playground_escape;

namespace
{
/// A story game on the island, with no fade and no script's wide screen
constexpr Gates k_OnTheIsland {.land = k_PlaygroundLand};

/// Records each step of the restart as it is taken
class RecordedSteps final: public RestartSteps
{
public:
	void DeleteScriptThings() override { taken.emplace_back("delete script things"); }
	void StopAllScripts() override { taken.emplace_back("stop scripts"); }
	void CleanForRestart() override { taken.emplace_back("clean"); }
	void StartScript(std::string_view script) override { taken.push_back("start " + std::string(script)); }
	void LessonStopped() override { taken.emplace_back("lesson stopped"); }
	void InterfaceToNormal() override { taken.emplace_back("interface normal"); }

	std::vector<std::string> taken;
};
} // namespace

TEST(PlaygroundEscape, DuringALessonEscapeStopsItAndWithoutOneItAsksToLeave)
{
	EXPECT_EQ(OnEscape(k_OnTheIsland, true), Outcome::StopLesson);
	EXPECT_EQ(OnEscape(k_OnTheIsland, false), Outcome::AskToLeave);
}

TEST(PlaygroundEscape, TheOtherLandsDoNotTakeTheKey)
{
	for (int32_t land = 1; land <= 5; ++land)
	{
		auto gates = k_OnTheIsland;
		gates.land = land;
		EXPECT_EQ(OnEscape(gates, true), Outcome::NotTaken) << "land " << land;
	}
}

TEST(PlaygroundEscape, AGameOverOrANetworkGameDoesNotTakeIt)
{
	auto over = k_OnTheIsland;
	over.gameOver = true;
	EXPECT_EQ(OnEscape(over, true), Outcome::NotTaken);
	auto network = k_OnTheIsland;
	network.networkGame = true;
	EXPECT_EQ(OnEscape(network, true), Outcome::NotTaken);
}

TEST(PlaygroundEscape, AWideScreenOrAFadeTakesTheKeyForNothing)
{
	auto wideScreen = k_OnTheIsland;
	wideScreen.scriptWideScreen = true;
	EXPECT_EQ(OnEscape(wideScreen, true), Outcome::Nothing);
	EXPECT_EQ(OnEscape(wideScreen, false), Outcome::Nothing);
	auto moving = k_OnTheIsland;
	moving.fadeMoving = true;
	EXPECT_EQ(OnEscape(moving, true), Outcome::Nothing);
	auto drawn = k_OnTheIsland;
	drawn.fadeDrawn = true;
	EXPECT_EQ(OnEscape(drawn, false), Outcome::Nothing);
}

TEST(PlaygroundEscape, TheRestartDeletesStopsCleansAndStartsTheFreePlay)
{
	RecordedSteps steps;
	RestartOnFreePlay(steps);
	const std::vector<std::string> expected {
	    "delete script things", "stop scripts", "clean", "start KeepRunningControl", "lesson stopped", "interface normal",
	};
	EXPECT_EQ(steps.taken, expected);
}

TEST(PlaygroundEscape, TheLessonsAreTheIslandsFifthDeveloperFunction)
{
	EXPECT_EQ(k_LessonDevFunction, 5);
	EXPECT_EQ(k_PlaygroundLand, 6);
}
