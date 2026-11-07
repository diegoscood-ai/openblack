/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// src/Graphics/InstanceRuns.h: a range of instance rows split into one batch per run of drawn rows. Synthetic masks.

#include <cstdint>

#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>

#include "Graphics/InstanceRuns.h"

using openblack::graphics::instance_runs::Part;
using openblack::graphics::instance_runs::Run;
using openblack::graphics::instance_runs::Runs;
using openblack::graphics::instance_runs::Split;
using R = std::vector<Run>;

TEST(InstanceRuns, anEmptyMaskDrawsTheWholeRangeAsOneRun)
{
	EXPECT_EQ(Runs({}, 5, 3), (R {{.offset = 5, .count = 3}}));
	EXPECT_EQ(Runs({}, 0, 1), (R {{.offset = 0, .count = 1}}));
	// an empty range has no run
	EXPECT_TRUE(Runs({}, 7, 0).empty());
}

TEST(InstanceRuns, rowsLeftOutSplitTheRange)
{
	//                                 0  1  2  3  4  5  6  7
	const std::vector<uint8_t> mask {1, 0, 1, 1, 0, 0, 1, 1};
	// rows 1 to 7: 2-3 and 6-7
	EXPECT_EQ(Runs(mask, 1, 7), (R {{.offset = 2, .count = 2}, {.offset = 6, .count = 2}}));
	// the whole mask
	EXPECT_EQ(Runs(mask, 0, 8), (R {{.offset = 0, .count = 1}, {.offset = 2, .count = 2}, {.offset = 6, .count = 2}}));
	// a range of left-out rows only
	EXPECT_TRUE(Runs(mask, 4, 2).empty());
}

TEST(InstanceRuns, rowsPastTheMaskAreDrawn)
{
	const std::vector<uint8_t> mask {1, 1, 0};
	// rows 1 to 5: 1, then 3-5 past the mask's end
	EXPECT_EQ(Runs(mask, 1, 5), (R {{.offset = 1, .count = 1}, {.offset = 3, .count = 3}}));
	// a range wholly past the end
	EXPECT_EQ(Runs(mask, 10, 4), (R {{.offset = 10, .count = 4}}));
}

TEST(InstanceRuns, anAllDrawnMaskGivesOneRun)
{
	const std::vector<uint8_t> mask(16, 1);
	EXPECT_EQ(Runs(mask, 3, 9), (R {{.offset = 3, .count = 9}}));
}

TEST(InstanceRuns, aRunWithNoForcedRowIsOnePart)
{
	EXPECT_EQ(Split({.offset = 4, .count = 6}, {}), (std::vector<Part> {{.offset = 4, .count = 6}}));
	// forced rows outside the run do not split it
	EXPECT_EQ(Split({.offset = 4, .count = 6}, {{3, 200}, {10, 200}}), (std::vector<Part> {{.offset = 4, .count = 6}}));
	EXPECT_TRUE(Split({.offset = 4, .count = 0}, {{4, 200}}).empty());
}

TEST(InstanceRuns, forcedRowsAreDrawnOnTheirOwn)
{
	const std::unordered_map<uint32_t, uint8_t> forced {{5, 230}, {6, 254}, {9, 180}};
	EXPECT_EQ(Split({.offset = 4, .count = 6}, forced), (std::vector<Part> {
	                                                        {.offset = 4, .count = 1},
	                                                        {.offset = 5, .count = 1, .forcedAlphaRef = 230},
	                                                        {.offset = 6, .count = 1, .forcedAlphaRef = 254},
	                                                        {.offset = 7, .count = 2},
	                                                        {.offset = 9, .count = 1, .forcedAlphaRef = 180},
	                                                    }));
	// a run of one forced row
	EXPECT_EQ(Split({.offset = 6, .count = 1}, forced), (std::vector<Part> {{.offset = 6, .count = 1, .forcedAlphaRef = 254}}));
}
