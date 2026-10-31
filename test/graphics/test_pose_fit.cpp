/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <optional>

#include <gtest/gtest.h>

#include "Graphics/DetailLevel.h"
#include "Graphics/PoseFit.h"

using openblack::graphics::pose_fit::PoseFits;
namespace detail_level = openblack::graphics::detail_level;

TEST(PoseFit, ThePoseOfItsOwnMeshFits)
{
	// a std mesh of 21 bones whose vertices use them all
	EXPECT_TRUE(PoseFits(21, 20));
}

TEST(PoseFit, AFinerMeshWithMoreBonesTakesTheStdPoseByIndex)
{
	// EGPT_M_B_1 and TIBT_M_B_1 have 22 bones against 21 in their std meshes, and their vertices use bones 0 to 20
	EXPECT_TRUE(PoseFits(21, 20));
	// a vertex on a bone the pose has no matrix for: the mesh is drawn at rest, as before
	EXPECT_FALSE(PoseFits(21, 21));
}

TEST(PoseFit, NoPoseNeverFits)
{
	EXPECT_FALSE(PoseFits(0, std::nullopt));
	EXPECT_FALSE(PoseFits(0, 0));
	// a mesh with no skinned vertex takes any pose
	EXPECT_TRUE(PoseFits(5, std::nullopt));
}

TEST(DetailLevel, OnlyTheTopLevelDrawsTheHighMeshes)
{
	// the user's decision of 2026-10-15: level 6 only; 4 is the default, 5 the custom one
	for (uint8_t level = 0; level < detail_level::k_Top; ++level)
	{
		EXPECT_FALSE(detail_level::DrawsHighMeshes(level)) << static_cast<int>(level);
	}
	EXPECT_TRUE(detail_level::DrawsHighMeshes(detail_level::k_Top));
	EXPECT_EQ(detail_level::k_Top, 6);
	EXPECT_FALSE(detail_level::DrawsHighMeshes(detail_level::k_Default));
}
