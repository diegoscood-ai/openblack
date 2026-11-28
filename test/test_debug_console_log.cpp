/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The debug Console's log sink: the lines a logger sends, named by logger and level, kept up to a bound and taken once

#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <spdlog/logger.h>

#include "Debug/ConsoleLogSink.h"

using openblack::debug::gui::ConsoleLogSink;

TEST(ConsoleLogSink, TheLoggedLinesAreTakenOnceOldestFirstWithTheirLoggerAndLevel)
{
	auto sink = std::make_shared<ConsoleLogSink>();
	spdlog::logger logger("game", sink);
	logger.set_level(spdlog::level::info);
	logger.info("first {}", 1);
	logger.debug("below the logger's level");
	logger.error("second");
	EXPECT_EQ(sink->Take(), (std::vector<std::string> {"[game] [info] first 1", "[game] [error] second"}));
	EXPECT_TRUE(sink->Take().empty());
}

TEST(ConsoleLogSink, ALogNotTakenKeepsOnlyItsNewestLines)
{
	auto sink = std::make_shared<ConsoleLogSink>(2);
	spdlog::logger logger("audio", sink);
	logger.warn("a");
	logger.warn("b");
	logger.warn("c");
	EXPECT_EQ(sink->Take(), (std::vector<std::string> {"[audio] [warning] b", "[audio] [warning] c"}));
}
