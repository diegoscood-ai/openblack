/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Graphics/DetailLevel.h"

namespace detail_level = openblack::graphics::detail_level;

TEST(DetailLevel, LowLevelsHaveNoSkyEffects)
{
	for (uint8_t level = 0; level < 3; ++level)
	{
		EXPECT_FALSE(detail_level::Fog(level));
		EXPECT_FALSE(detail_level::Clouds(level));
		EXPECT_FALSE(detail_level::Weather(level));
	}
	EXPECT_TRUE(detail_level::Fog(detail_level::k_Default));
	EXPECT_TRUE(detail_level::Clouds(detail_level::k_Default));
	EXPECT_TRUE(detail_level::Weather(detail_level::k_Default));
}

TEST(DetailLevel, SeaTilingAndPastTheLastLevel)
{
	EXPECT_FLOAT_EQ(detail_level::WaterTiling(0), 0.0f);
	EXPECT_FLOAT_EQ(detail_level::WaterTiling(5), 0.5f);
	EXPECT_FLOAT_EQ(detail_level::Get(4).SeaPeriod(), 2000.0f - 1800.0f * 0.8f);
	// A level past the last is the last
	EXPECT_FLOAT_EQ(detail_level::WaterTiling(9), 1.0f);
	EXPECT_EQ(&detail_level::Get(200), &detail_level::Get(6));
}
