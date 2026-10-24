/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// A land block's highest point as the land opens (LandBlock::HighestAltitude): the largest altitude byte of its 17 x 17
// cells, whatever the file stored, the border row and column included; on a map with more altitude bits, the whole
// altitude.

#include <cstdint>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandBlock.h"

using namespace openblack;

TEST(LandBlockHighest, TheLargestAltitudeByteOfTheCellsWhateverTheFileStored)
{
	lnd::LNDBlock block {};
	block.highestAltitude = 1;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 8), 0u);
	block.cells[5 * 17 + 9].altitude = 122;
	block.cells[3].altitude = 53;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 8), 122u);
	// the shared border row and column count too
	block.cells[16 * 17 + 16].altitude = 200;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 8), 200u);
	// with 8 altitude bits the save colour holds no altitude
	block.cells[0].saveColor = 0x03;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 8), 200u);
	block.cells[0].altitude = 255;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 8), 255u);
	// with 10 its two low bits are the altitude's high bits
	block.cells[0].altitude = 4;
	EXPECT_EQ(LandBlock::HighestAltitude(block, 10), 0x304u);
}
