/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/MaterialStages.h"

using namespace openblack::graphics::material_stages;

TEST(MaterialStages, TheHighestThreeBitsChooseHowTheEnvironmentMapIsCombined)
{
	EXPECT_EQ(EnvironmentStageOf(0x00), EnvironmentStage::Off);
	EXPECT_EQ(EnvironmentStageOf(0x14), EnvironmentStage::Off);
	EXPECT_EQ(EnvironmentStageOf(0x20), EnvironmentStage::Kept);
	EXPECT_EQ(EnvironmentStageOf(0x54), EnvironmentStage::Added);
	EXPECT_EQ(EnvironmentStageOf(0x60), EnvironmentStage::Modulated);
	EXPECT_EQ(EnvironmentStageOf(0x80), EnvironmentStage::Modulated2);
	EXPECT_EQ(EnvironmentStageOf(0xA0), EnvironmentStage::Map);
	EXPECT_EQ(EnvironmentStageOf(0xC0), EnvironmentStage::Off);
	EXPECT_EQ(EnvironmentStageOf(0xFF), EnvironmentStage::Off);
}

TEST(MaterialStages, ThePhialsGlassAloneTakesTheMapAdded)
{
	// The phials' four materials: stopper 0x14, liquid 0x04, glass 0x54, label 0x14
	EXPECT_EQ(EnvironmentStageOf(0x14), EnvironmentStage::Off);
	EXPECT_EQ(EnvironmentStageOf(0x04), EnvironmentStage::Off);
	EXPECT_EQ(EnvironmentStageOf(0x54), EnvironmentStage::Added);
}
