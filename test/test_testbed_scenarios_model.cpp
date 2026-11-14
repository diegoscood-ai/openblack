/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The Testbed Scenarios window's model (Debug/TestbedScenariosModel): which facets and scenarios it lists, what stays
// picked, its status line, the scenario seconds of a frame and the run the command line asks for. Pure: hand-made
// lists of scenarios, no game.

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include "Debug/TestbedScenariosModel.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
Scenario Make(std::string_view id, Facet facet, bool crowd = false)
{
	Scenario scenario {.id = id, .name = id, .facet = facet};
	if (crowd)
	{
		scenario.crowd = Crowd {.count = 10};
	}
	return scenario;
}

/// Two of Idle, one of Needs, one of Benchmark; the other facets have none
std::vector<Scenario> MakeList()
{
	return {
	    Make("idle.a", Facet::Idle),
	    Make("needs.a", Facet::Needs),
	    Make("idle.b", Facet::Idle),
	    Make("benchmark.a", Facet::Benchmark, true),
	};
}
} // namespace

TEST(TestbedScenariosModel, OnlyFacetsWithScenariosAreListed)
{
	const auto all = MakeList();
	EXPECT_EQ(ListedFacets(all), (std::vector<Facet> {Facet::Idle, Facet::Needs, Facet::Benchmark}));
	EXPECT_TRUE(ListedFacets({}).empty());
}

TEST(TestbedScenariosModel, ListedNarrowsToTheFacet)
{
	const auto all = MakeList();
	EXPECT_EQ(Listed(all, std::nullopt), (std::vector<size_t> {0, 1, 2, 3}));
	EXPECT_EQ(Listed(all, Facet::Idle), (std::vector<size_t> {0, 2}));
	EXPECT_EQ(Listed(all, Facet::Needs), (std::vector<size_t> {1}));
	EXPECT_TRUE(Listed(all, Facet::Hand).empty());
}

TEST(TestbedScenariosModel, PickStaysWhenTheFacetHasIt)
{
	const auto all = MakeList();
	EXPECT_EQ(PickAfterFacetChange(all, Facet::Idle, 2), 2u);
	EXPECT_EQ(PickAfterFacetChange(all, std::nullopt, 3), 3u);
}

TEST(TestbedScenariosModel, PickMovesToTheFacetsFirstOtherwise)
{
	const auto all = MakeList();
	EXPECT_EQ(PickAfterFacetChange(all, Facet::Idle, 1), 0u);
	EXPECT_EQ(PickAfterFacetChange(all, Facet::Benchmark, 0), 3u);
}

TEST(TestbedScenariosModel, PickIsKeptForAFacetWithNone)
{
	const auto all = MakeList();
	EXPECT_EQ(PickAfterFacetChange(all, Facet::Hand, 1), 1u);
}

TEST(TestbedScenariosModel, PickedIsKeptWithinTheList)
{
	const auto all = MakeList();
	EXPECT_EQ(PickedIn(all, 1), std::optional<size_t>(1));
	EXPECT_EQ(PickedIn(all, 9), std::optional<size_t>(3));
	// An empty list has nothing to pick, rather than an index before its start
	EXPECT_FALSE(PickedIn({}, 0).has_value());
}

TEST(TestbedScenariosModel, ScenarioSecondsFollowTheGame)
{
	EXPECT_FLOAT_EQ(ScenarioSeconds(0.1f, false, 1.0f), 0.1f);
	EXPECT_FLOAT_EQ(ScenarioSeconds(0.1f, false, 0.5f), 0.2f);
	EXPECT_FLOAT_EQ(ScenarioSeconds(0.1f, false, 2.0f), 0.05f);
	EXPECT_EQ(ScenarioSeconds(0.1f, true, 1.0f), 0.0f);
	// A speed of nothing is taken as the fastest there is, never a division by 0
	EXPECT_NEAR(ScenarioSeconds(0.1f, false, 0.0f), 10.0f, 1e-4f);
}

TEST(TestbedScenariosModel, StatusLineSaysHowFarTheCommandsHaveGot)
{
	auto scenario = Make("idle.a", Facet::Idle);
	Timeline timeline;
	EXPECT_EQ(StatusLine(true, scenario, 1.5f, timeline), "Running idle.a, 1.5 s in, no commands");

	scenario.commands.resize(3);
	timeline.next = 1;
	EXPECT_EQ(StatusLine(true, scenario, 2.0f, timeline), "Running idle.a, 2.0 s in, next command 2 of 3");

	timeline.done = true;
	EXPECT_EQ(StatusLine(false, scenario, 12.5f, timeline), "Stopped idle.a, 12.5 s in, every command given");
}

TEST(TestbedScenariosModel, StrongestDesireIsTheLargestActivated)
{
	creature_desires::Desires desires;
	EXPECT_FALSE(StrongestDesire(desires).has_value());

	desires.desires.at(1).value = 0.4f;
	desires.desires.at(2).value = 0.7f;
	desires.desires.at(3).value = 0.9f;
	desires.desires.at(3).activated = false;
	EXPECT_EQ(StrongestDesire(desires), static_cast<creature_desires::Desire>(2));
}

TEST(TestbedScenariosModel, UnknownIdIsNoRun)
{
	const auto all = MakeList();
	EXPECT_FALSE(ResolveRequest(all, {.id = "nothing.such"}).has_value());
	EXPECT_FALSE(ResolveRequest({}, {.id = "idle.a"}).has_value());
	EXPECT_EQ(KnownIds(all), " idle.a needs.a idle.b benchmark.a");
}

TEST(TestbedScenariosModel, ScenarioWithoutACrowdNeitherQuitsNorWrites)
{
	const auto all = MakeList();
	const auto run = ResolveRequest(all, {.id = "idle.b", .warmUpFrames = 5, .frames = 7, .results = "out/run"});
	ASSERT_TRUE(run.has_value());
	EXPECT_EQ(run->index, 2u);
	EXPECT_EQ(run->settings.warmUpFrames, 5u);
	EXPECT_EQ(run->settings.frames, 7u);
	EXPECT_FALSE(run->settings.quitWhenMeasured);
	EXPECT_FALSE(run->settings.resultsBase.has_value());
}

TEST(TestbedScenariosModel, BenchmarkQuitsAndWritesOnlyWhereAsked)
{
	const auto all = MakeList();
	const auto written = ResolveRequest(all, {.id = "benchmark.a", .results = "out/run"});
	ASSERT_TRUE(written.has_value());
	EXPECT_EQ(written->index, 3u);
	EXPECT_TRUE(written->settings.quitWhenMeasured);
	ASSERT_TRUE(written->settings.resultsBase.has_value());
	EXPECT_EQ(*written->settings.resultsBase, std::filesystem::path("out/run"));

	// Without --benchmark-out it is measured and quits, but nothing is written
	const auto unwritten = ResolveRequest(all, {.id = "benchmark.a"});
	ASSERT_TRUE(unwritten.has_value());
	EXPECT_TRUE(unwritten->settings.quitWhenMeasured);
	EXPECT_FALSE(unwritten->settings.resultsBase.has_value());
}

TEST(TestbedScenariosModel, RunFromTheWindowCarriesOn)
{
	const auto settings = ForRunFromWindow({.warmUpFrames = 3, .frames = 4, .quitWhenMeasured = true, .resultsBase = "x"});
	EXPECT_EQ(settings.warmUpFrames, 3u);
	EXPECT_EQ(settings.frames, 4u);
	EXPECT_FALSE(settings.quitWhenMeasured);
	EXPECT_EQ(settings.resultsBase, std::optional<std::filesystem::path>("x"));
}
