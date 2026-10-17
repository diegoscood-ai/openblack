/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's command line options (Debug/TestbedOptions): --scenario makes the request, the benchmark's frames and
// results path go with it, and nothing is asked for without it; --scenarios makes a list of them, each with its frames,
// its state hash file and its shot 20 frames before its end; a land entry (land:<file>:<frames>[:key=value]...) is a land
// run with its test options, named after its demo or its land. Pure: no game.

#include <filesystem>
#include <optional>
#include <string>

#include <gtest/gtest.h>

#include "Debug/TestbedOptions.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

TEST(TestbedOptions, NoScenarioAsksForNothing)
{
	EXPECT_FALSE(MakeScenarioRequest(std::nullopt, 120, 600, std::nullopt).has_value());
	// A results path alone runs nothing and writes nothing
	EXPECT_FALSE(MakeScenarioRequest(std::nullopt, 120, 600, std::string("benchmarks/out")).has_value());
}

TEST(TestbedOptions, ScenarioKeepsItsIdAndFrames)
{
	const auto request = MakeScenarioRequest(std::string("benchmark.creatures_100"), 30, 250, std::nullopt);
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->id, "benchmark.creatures_100");
	EXPECT_EQ(request->warmUpFrames, 30u);
	EXPECT_EQ(request->frames, 250u);
	EXPECT_FALSE(request->results.has_value());
}

TEST(TestbedOptions, DefaultsMatchTheRequestsOwn)
{
	const ScenarioRequest defaults;
	const auto request = MakeScenarioRequest(std::string("idle.fidget"), defaults.warmUpFrames, defaults.frames, std::nullopt);
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->warmUpFrames, 120u);
	EXPECT_EQ(request->frames, 600u);
}

TEST(TestbedOptions, AtLeastOneFrameIsMeasured)
{
	const auto request = MakeScenarioRequest(std::string("benchmark.villagers_500"), 0, 0, std::nullopt);
	ASSERT_TRUE(request.has_value());
	EXPECT_EQ(request->frames, 1u);
	// No settling is allowed
	EXPECT_EQ(request->warmUpFrames, 0u);
}

TEST(TestbedOptions, ResultsPathIsKeptAsGiven)
{
	const auto request = MakeScenarioRequest(std::string("benchmark.creatures_100"), 120, 600, std::string("runs/a.b"));
	ASSERT_TRUE(request.has_value());
	ASSERT_TRUE(request->results.has_value());
	EXPECT_EQ(*request->results, std::filesystem::path("runs/a.b"));
}

TEST(TestbedScenarioList, EachIdIsAnEntryWithItsFramesAndOutputs)
{
	const auto list = MakeScenarioList("examples.storm_lightning:600,examples.leashed_creature", 900, "C:/audit/batch_", 120,
	                                   600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 2u);
	const auto& first = list->at(0);
	EXPECT_EQ(first.request.id, "examples.storm_lightning");
	EXPECT_EQ(first.frames, 600u);
	EXPECT_EQ(first.out, std::filesystem::path("C:/audit/batch_examples.storm_lightning"));
	EXPECT_EQ(first.stateHash, std::filesystem::path("C:/audit/batch_examples.storm_lightning_hash.txt"));
	ASSERT_TRUE(first.screenshot.has_value());
	// the shot 20 frames before the end, as a single run takes it
	EXPECT_EQ(first.screenshot->first, 580u);
	EXPECT_EQ(first.screenshot->second, std::filesystem::path("C:/audit/batch_examples.storm_lightning.png"));
	// without its own frames, an entry runs the default ones
	const auto& second = list->at(1);
	EXPECT_EQ(second.request.id, "examples.leashed_creature");
	EXPECT_EQ(second.frames, 900u);
	ASSERT_TRUE(second.screenshot.has_value());
	EXPECT_EQ(second.screenshot->first, 880u);
}

TEST(TestbedScenarioList, TheBenchmarkSettingsGoWithEveryEntry)
{
	const auto list = MakeScenarioList("benchmark.creatures_100,idle.fidget", 900, "", 30, 250, std::string("runs/b"));
	ASSERT_TRUE(list.has_value());
	for (const auto& entry : *list)
	{
		EXPECT_EQ(entry.request.warmUpFrames, 30u);
		EXPECT_EQ(entry.request.frames, 250u);
		ASSERT_TRUE(entry.request.results.has_value());
		EXPECT_EQ(*entry.request.results, std::filesystem::path("runs/b"));
	}
}

TEST(TestbedScenarioList, AnIdGivenAgainNamesItsOutputsByItsCount)
{
	const auto list = MakeScenarioList("a.b,c.d,a.b,a.b", 100, "x_", 120, 600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 4u);
	EXPECT_EQ(list->at(0).out, std::filesystem::path("x_a.b"));
	EXPECT_EQ(list->at(1).out, std::filesystem::path("x_c.d"));
	EXPECT_EQ(list->at(2).out, std::filesystem::path("x_a.b.2"));
	EXPECT_EQ(list->at(3).out, std::filesystem::path("x_a.b.3"));
	EXPECT_EQ(list->at(3).stateHash, std::filesystem::path("x_a.b.3_hash.txt"));
	// every entry still runs the scenario of its id
	EXPECT_EQ(list->at(3).request.id, "a.b");
}

TEST(TestbedScenarioList, AShortEntryHasNoShot)
{
	const auto list = MakeScenarioList("a.b:20,c.d:21", 900, "", 120, 600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	EXPECT_FALSE(list->at(0).screenshot.has_value());
	ASSERT_TRUE(list->at(1).screenshot.has_value());
	EXPECT_EQ(list->at(1).screenshot->first, 1u);
}

TEST(TestbedScenarioList, AMalformedListAsksForNothing)
{
	for (const auto* text : {"", ",", "a.b,", ",a.b", "a.b,,c.d", "a.b:", "a.b:0", "a.b:x", "a.b:12x", "a.b:-5", ":100"})
	{
		EXPECT_FALSE(MakeScenarioList(text, 900, "", 120, 600, std::nullopt).has_value()) << text;
	}
	// with no frames of its own and none by default, an entry would never end
	EXPECT_FALSE(MakeScenarioList("a.b", 0, "", 120, 600, std::nullopt).has_value());
}

TEST(TestbedLandEntry, ALandEntryKeepsItsLandFramesAndOptions)
{
	std::string error;
	const auto entry = ParseLandEntry("land:Land1.txt:900:demo=Gestures:skip=3/30", error);
	ASSERT_TRUE(entry.has_value()) << error;
	EXPECT_EQ(entry->run.land, "Land1.txt");
	EXPECT_EQ(entry->frames, 900u);
	ASSERT_TRUE(entry->run.handDemo.has_value());
	EXPECT_EQ(*entry->run.handDemo, "Gestures");
	ASSERT_TRUE(entry->run.skipAnswer.has_value());
	EXPECT_EQ(entry->run.skipAnswer->answer, 3);
	EXPECT_EQ(entry->run.skipAnswer->frame, 30u);
	EXPECT_TRUE(error.empty());
}

TEST(TestbedLandEntry, OptionsAreOptionalAndInAnyOrder)
{
	std::string error;
	const auto bare = ParseLandEntry("land:Land2.txt:50", error);
	ASSERT_TRUE(bare.has_value()) << error;
	EXPECT_FALSE(bare->run.handDemo.has_value());
	EXPECT_FALSE(bare->run.skipAnswer.has_value());
	const auto swapped = ParseLandEntry("land:Playgrounds/TwoGods.txt:900:skip=0/0:demo=castfood", error);
	ASSERT_TRUE(swapped.has_value()) << error;
	EXPECT_EQ(swapped->run.land, "Playgrounds/TwoGods.txt");
	EXPECT_EQ(*swapped->run.handDemo, "castfood");
	EXPECT_EQ(swapped->run.skipAnswer->answer, 0);
	EXPECT_EQ(swapped->run.skipAnswer->frame, 0u);
}

TEST(TestbedLandEntry, MenuOpensTheEscapeMenuAtTheRunsFrame)
{
	std::string error;
	const auto entry = ParseLandEntry("land:Land1.txt:760:menu=600", error);
	ASSERT_TRUE(entry.has_value()) << error;
	ASSERT_TRUE(entry->run.menuAt.has_value());
	EXPECT_EQ(*entry->run.menuAt, 600u);
	// frame 0 is a frame like any other
	const auto first = ParseLandEntry("land:Land1.txt:760:skip=3/30:menu=0", error);
	ASSERT_TRUE(first.has_value()) << error;
	EXPECT_EQ(*first->run.menuAt, 0u);
	// without menu= the run leaves OPENBLACK_TEST_MENU_AT as it is
	const auto bare = ParseLandEntry("land:Land1.txt:760", error);
	ASSERT_TRUE(bare.has_value()) << error;
	EXPECT_FALSE(bare->run.menuAt.has_value());
}

TEST(TestbedLandEntry, CitadelGivesTheTemplesHookTurns)
{
	std::string error;
	const auto in = ParseLandEntry("land:Land1.txt:720:citadel=120", error);
	ASSERT_TRUE(in.has_value()) << error;
	ASSERT_TRUE(in->run.citadel.has_value());
	EXPECT_EQ(in->run.citadel->in, 120u);
	// without a way out, as the variable's "120" alone
	EXPECT_EQ(in->run.citadel->out, 0u);
	const auto inOut = ParseLandEntry("land:Land1.txt:720:citadel=120/200", error);
	ASSERT_TRUE(inOut.has_value()) << error;
	EXPECT_EQ(inOut->run.citadel->in, 120u);
	EXPECT_EQ(inOut->run.citadel->out, 200u);
	const auto bare = ParseLandEntry("land:Land1.txt:720", error);
	ASSERT_TRUE(bare.has_value()) << error;
	EXPECT_FALSE(bare->run.citadel.has_value());
}

TEST(TestbedLandEntry, TheKeyTableListsDemoSkipMenuThenCitadel)
{
	const auto options = LandOptions();
	ASSERT_EQ(options.size(), 4u);
	EXPECT_EQ(options[0].key, "demo");
	EXPECT_EQ(options[1].key, "skip");
	EXPECT_EQ(options[2].key, "menu");
	EXPECT_EQ(options[3].key, "citadel");
	for (const auto& option : options)
	{
		EXPECT_FALSE(option.value.empty()) << option.key;
		EXPECT_NE(option.apply, nullptr) << option.key;
	}
}

TEST(TestbedLandEntry, AnUnknownKeyListsTheKnownOnes)
{
	std::string error;
	EXPECT_FALSE(ParseLandEntry("land:Land1.txt:900:tod=22", error).has_value());
	EXPECT_NE(error.find("tod=22"), std::string::npos) << error;
	EXPECT_NE(error.find("demo=<name>, skip=<answer>/<frame>, menu=<frame>, citadel=<in>[/<out>]"), std::string::npos) << error;
	// a key without a value is unknown too
	error.clear();
	EXPECT_FALSE(ParseLandEntry("land:Land1.txt:900:demo", error).has_value());
	EXPECT_NE(error.find("known: demo="), std::string::npos) << error;
}

TEST(TestbedLandEntry, AMalformedEntryIsRefusedWithItsReason)
{
	for (const auto* text : {"land:",
	                         "land:Land1.txt",
	                         "land::900",
	                         "land:Land1.txt:",
	                         "land:Land1.txt:0",
	                         "land:Land1.txt:9x",
	                         "land:Land1.txt:-1",
	                         "land:/Land1.txt:900",
	                         "land:Land 1.txt:900",
	                         "land:Land1.txt:900:",
	                         "land:Land1.txt:900:demo=",
	                         "land:Land1.txt:900:demo=a b",
	                         "land:Land1.txt:900:skip=3",
	                         "land:Land1.txt:900:skip=3,30",
	                         "land:Land1.txt:900:skip=4/30",
	                         "land:Land1.txt:900:skip=-1/30",
	                         "land:Land1.txt:900:skip=3/x",
	                         "land:Land1.txt:900:skip=/30",
	                         "land:Land1.txt:900:demo=a:demo=b",
	                         "land:Land1.txt:900:menu=",
	                         "land:Land1.txt:900:menu=x",
	                         "land:Land1.txt:900:menu=-1",
	                         "land:Land1.txt:900:menu=6/0",
	                         "land:Land1.txt:900:menu=1:menu=2",
	                         "land:Land1.txt:900:citadel=",
	                         "land:Land1.txt:900:citadel=0",
	                         "land:Land1.txt:900:citadel=x",
	                         "land:Land1.txt:900:citadel=120/",
	                         "land:Land1.txt:900:citadel=/200",
	                         "land:Land1.txt:900:citadel=120/0",
	                         "land:Land1.txt:900:citadel=120/120",
	                         "land:Land1.txt:900:citadel=120/100",
	                         "land:Land1.txt:900:citadel=120/200/300",
	                         "land:Land1.txt:900:citadel=-120",
	                         "land:Land1.txt:900:citadel=1:citadel=2",
	                         "Land1.txt:900"})
	{
		std::string error;
		EXPECT_FALSE(ParseLandEntry(text, error).has_value()) << text;
		EXPECT_FALSE(error.empty()) << text;
	}
}

TEST(TestbedLandEntry, ItsOutputsAreNamedByItsDemoElseItsLand)
{
	EXPECT_EQ(LandRunName({.land = "Land1.txt", .handDemo = "fireball"}), "fireball");
	EXPECT_EQ(LandRunName({.land = "Land1.txt"}), "Land1");
	EXPECT_EQ(LandRunName({.land = "Playgrounds/TwoGods.txt"}), "TwoGods");
}

TEST(TestbedScenarioList, ALandEntryIsALandRunWithTheSingleRunsOutputs)
{
	const auto list = MakeScenarioList("land:Land1.txt:900:demo=Gestures:skip=3/30,land:Land1.txt:900:demo=drag", 0,
	                                   "C:/audit/b_", 120, 600, std::string("runs/b"));
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 2u);
	const auto& first = list->at(0);
	// no scenario: no id and none of the benchmark's settings
	EXPECT_TRUE(first.request.id.empty());
	EXPECT_FALSE(first.request.results.has_value());
	ASSERT_TRUE(first.request.land.has_value());
	EXPECT_EQ(first.request.land->land, "Land1.txt");
	EXPECT_EQ(*first.request.land->handDemo, "Gestures");
	EXPECT_EQ(first.request.land->skipAnswer->frame, 30u);
	// its own frames, whatever -n says, and the outputs a single hand demo run of the same label names
	EXPECT_EQ(first.frames, 900u);
	EXPECT_EQ(first.out, std::filesystem::path("C:/audit/b_Gestures"));
	EXPECT_EQ(first.stateHash, std::filesystem::path("C:/audit/b_Gestures_hash.txt"));
	ASSERT_TRUE(first.screenshot.has_value());
	EXPECT_EQ(first.screenshot->first, 880u);
	EXPECT_EQ(first.screenshot->second, std::filesystem::path("C:/audit/b_Gestures.png"));
	EXPECT_EQ(list->at(1).out, std::filesystem::path("C:/audit/b_drag"));
	EXPECT_FALSE(list->at(1).request.land->skipAnswer.has_value());
}

TEST(TestbedScenarioList, LandEntriesAndScenariosMixAndCountTheirNames)
{
	const auto list = MakeScenarioList("land:Land1.txt:900:demo=shield,a.b,land:Land1.txt:900:demo=shield,land:Land2.txt:100",
	                                   900, "x_", 120, 600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 4u);
	EXPECT_EQ(list->at(0).out, std::filesystem::path("x_shield"));
	EXPECT_EQ(list->at(1).request.id, "a.b");
	EXPECT_FALSE(list->at(1).request.land.has_value());
	EXPECT_EQ(list->at(2).out, std::filesystem::path("x_shield.2"));
	EXPECT_EQ(list->at(3).out, std::filesystem::path("x_Land2"));
	EXPECT_EQ(list->at(3).frames, 100u);
}

TEST(TestbedScenarioList, AMalformedListSaysWhy)
{
	std::string error;
	EXPECT_FALSE(MakeScenarioList("a.b,land:Land1.txt:900:tod=22", 900, "", 120, 600, std::nullopt, &error).has_value());
	EXPECT_NE(error.find("tod=22"), std::string::npos) << error;
	EXPECT_NE(error.find("known: demo="), std::string::npos) << error;
	error.clear();
	EXPECT_FALSE(MakeScenarioList("a.b:0", 900, "", 120, 600, std::nullopt, &error).has_value());
	EXPECT_FALSE(error.empty());
	// a skip= with a comma splits the list: its second half is a bad entry
	EXPECT_FALSE(MakeScenarioList("land:Land1.txt:900:skip=3,30", 900, "", 120, 600, std::nullopt).has_value());
}

TEST(TestbedScenarioList, TheMenuAndTheLandViewRunAsTwoLandEntries)
{
	// the menu shot and the land view of one launch: each its own new game on Land 1, the shot at frame 740 of 760
	const auto list =
	    MakeScenarioList("land:Land1.txt:760:menu=600,land:Land1.txt:760", 0, "C:/audit/v_", 120, 600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 2u);
	EXPECT_EQ(*list->at(0).request.land->menuAt, 600u);
	EXPECT_FALSE(list->at(1).request.land->menuAt.has_value());
	EXPECT_EQ(list->at(0).out, std::filesystem::path("C:/audit/v_Land1"));
	EXPECT_EQ(list->at(1).out, std::filesystem::path("C:/audit/v_Land1.2"));
	for (const auto& entry : *list)
	{
		EXPECT_EQ(entry.frames, 760u);
		ASSERT_TRUE(entry.screenshot.has_value());
		EXPECT_EQ(entry.screenshot->first, 740u);
	}
}

TEST(TestbedScenarioList, TheTemplePairRunsAsTwoLandEntriesWithTheirOwnTurns)
{
	// the temple's two shots of one launch: in at hook turn 120, and in at 120 and out at 200, each its own new game
	const auto list = MakeScenarioList("land:Land1.txt:720:citadel=120,land:Land1.txt:720:citadel=120/200", 0, "C:/shots/t_",
	                                   120, 600, std::nullopt);
	ASSERT_TRUE(list.has_value());
	ASSERT_EQ(list->size(), 2u);
	EXPECT_EQ(list->at(0).request.land->citadel->in, 120u);
	EXPECT_EQ(list->at(0).request.land->citadel->out, 0u);
	EXPECT_EQ(list->at(1).request.land->citadel->in, 120u);
	EXPECT_EQ(list->at(1).request.land->citadel->out, 200u);
	EXPECT_EQ(list->at(0).out, std::filesystem::path("C:/shots/t_Land1"));
	EXPECT_EQ(list->at(1).out, std::filesystem::path("C:/shots/t_Land1.2"));
	for (const auto& entry : *list)
	{
		ASSERT_TRUE(entry.screenshot.has_value());
		EXPECT_EQ(entry.screenshot->first, 700u);
	}
	// the variable's comma would split the list
	EXPECT_FALSE(MakeScenarioList("land:Land1.txt:720:citadel=120,200", 0, "", 120, 600, std::nullopt).has_value());
}
