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

#include <vector>

#include <gtest/gtest.h>

#include "Graphics/InstanceRuns.h"

using openblack::graphics::instance_runs::Run;
using openblack::graphics::instance_runs::Runs;
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
