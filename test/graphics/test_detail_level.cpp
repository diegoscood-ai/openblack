/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "ECS/DrawList/FrameInputs.h"
#include "Graphics/DetailLevel.h"

namespace detail_level = openblack::graphics::detail_level;
namespace draw_list = openblack::ecs::draw_list;

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

TEST(DetailLevel, TheGlareIsHiddenByWhatIsDrawnAtTheDetailIndicesThreeFourAndSix)
{
	for (int32_t index = 0; index < 3; ++index)
	{
		EXPECT_FALSE(detail_level::GlareDepthTest(index));
	}
	EXPECT_TRUE(detail_level::GlareDepthTest(3));
	EXPECT_TRUE(detail_level::GlareDepthTest(4));
	EXPECT_FALSE(detail_level::GlareDepthTest(5));
	EXPECT_TRUE(detail_level::GlareDepthTest(6));
	EXPECT_FALSE(detail_level::GlareDepthTest(7));
}

TEST(DetailLevel, TheGlareIsHiddenByWhatIsDrawnAtLevelsThreeToSix)
{
	// Through the index the original runs with: above 4 it is 4
	for (uint8_t level = 0; level < 3; ++level)
	{
		EXPECT_FALSE(detail_level::GlareDepthTest(draw_list::DetailIndexOf(level)));
	}
	for (uint8_t level = 3; level <= 6; ++level)
	{
		EXPECT_TRUE(detail_level::GlareDepthTest(draw_list::DetailIndexOf(level)));
	}
	EXPECT_TRUE(detail_level::GlareDepthTest(draw_list::DetailIndexOf(detail_level::k_Default)));
}

TEST(DetailLevel, TheOptionsBoxTopEntryIsTheTopLevel)
{
	// the original's control is the detail index itself; by the user's decision of 2026-10-20 its top entry, Maximum
	// Detail, is the top level 6, with the high meshes
	for (int entry = 0; entry < detail_level::k_MenuTopEntry; ++entry)
	{
		EXPECT_EQ(detail_level::LevelOfMenuEntry(entry), entry);
	}
	EXPECT_EQ(detail_level::LevelOfMenuEntry(detail_level::k_MenuTopEntry), detail_level::k_Top);
	EXPECT_EQ(detail_level::k_MenuEntries, 5);
}

TEST(DetailLevel, TheOptionsBoxShowsMaximumFromLevelFourUp)
{
	for (uint8_t level = 0; level < 4; ++level)
	{
		EXPECT_EQ(detail_level::MenuEntryOf(level), level);
	}
	// 4 (the default) and 5 have no entry of their own: Maximum, as the original shows its top entry at its default
	for (uint8_t level = 4; level <= detail_level::k_Top; ++level)
	{
		EXPECT_EQ(detail_level::MenuEntryOf(level), detail_level::k_MenuTopEntry);
	}
}

TEST(DetailLevel, MaximumIsTheTopLevelAndTheOthersChangeOnlyWhenStepped)
{
	constexpr int k_Maximum = detail_level::k_MenuTopEntry;
	// left on Maximum, also when it showed Maximum at the default 4 or the custom 5: the top level
	EXPECT_EQ(detail_level::LevelAfterOptions(detail_level::k_Default, k_Maximum, k_Maximum), detail_level::k_Top);
	EXPECT_EQ(detail_level::LevelAfterOptions(5, k_Maximum, k_Maximum), detail_level::k_Top);
	// left on another entry it opened with: the level stays
	EXPECT_EQ(detail_level::LevelAfterOptions(2, 2, 2), 2);
	// stepped onto Maximum from another entry: the top level
	EXPECT_EQ(detail_level::LevelAfterOptions(2, 2, k_Maximum), detail_level::k_Top);
	// stepped down from Maximum: that entry's level
	EXPECT_EQ(detail_level::LevelAfterOptions(detail_level::k_Default, k_Maximum, 3), 3);
	EXPECT_EQ(detail_level::LevelAfterOptions(detail_level::k_Top, k_Maximum, 0), 0);
}

TEST(DetailLevel, OpenblackStartsAtTheTopLevel)
{
	// the original starts at its default level; openblack at the top one, by the user's choice
	EXPECT_EQ(detail_level::k_Start, detail_level::k_Top);
	EXPECT_EQ(detail_level::k_Default, 4);
}
