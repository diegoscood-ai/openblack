/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's flat land under the creature's walkable mask: a creature wades the lake's shallows but not its open
// water

#include <gtest/gtest.h>

#include "3D/FlatLand.h"
#include "3D/LandAvoid.h"
#include "3D/LandAvoidState.h"
#include "ECS/Systems/LandAvoidSystemInterface.h"
#include "Locator.h"
#include "support/LandFakes.h"

using namespace openblack;

TEST(FlatLand, CreaturesWadeTheShallowsButNotTheOpenWater)
{
	test::WaterCellIsland island(LandData::k_BlocksPerSide);
	for (int x = 0; x < flat_land::k_CellsPerSide; ++x)
	{
		for (int z = 0; z < flat_land::k_CellsPerSide; ++z)
		{
			island.Cell(glm::u16vec2(x, z)) = flat_land::CellAt(x, z);
		}
	}

	auto& state = Locator::landAvoidSystem::value().GetState();
	const auto previous = state;
	land_avoid::Validate(island);

	// Open water is never walked: avoided next to the shallows, and out of reach further in
	EXPECT_EQ(land_avoid::At(flat_land::k_LakeMinX, flat_land::k_LakeMinZ + 5), land_avoid::k_Avoid);
	EXPECT_EQ(land_avoid::At(flat_land::k_LakeMinX + 5, flat_land::k_LakeMinZ + 5), land_avoid::k_Unreachable);
	EXPECT_EQ(land_avoid::At(flat_land::k_LakeMinX - 1, flat_land::k_LakeMinZ + 5), land_avoid::k_Water);
	EXPECT_EQ(land_avoid::At(flat_land::k_LakeMinX - 2, flat_land::k_LakeMinZ + 5), land_avoid::k_Water);
	EXPECT_EQ(land_avoid::At(flat_land::k_LakeMinX - 3, flat_land::k_LakeMinZ + 5), land_avoid::k_Land);
	EXPECT_EQ(land_avoid::At(256, 256), land_avoid::k_Land);
	// The middle of the near shallows is somewhere to stand
	const glm::vec2 shallows {flat_land::k_LakeCentre.x, flat_land::k_LakeCentre.y - flat_land::k_LakeHalfExtent.y - 10.0f};
	EXPECT_TRUE(land_avoid::IsPosValid({shallows.x, 0.0f, shallows.y}));

	state = previous;
}
