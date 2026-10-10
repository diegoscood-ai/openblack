/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's command line options (Debug/TestbedOptions): --scenario makes the request, the benchmark's frames and
// results path go with it, and nothing is asked for without it. Pure: no game.

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
