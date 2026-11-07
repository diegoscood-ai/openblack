/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "3D/L3DMesh.h"

using openblack::graphics::MeshPrintsFootprint;

namespace
{
// The header bit of a mesh with a landscape feature (the field's mesh, a building's)
constexpr uint32_t k_LandscapeFeature = 0x8000;
// The header bit that z-sorts the whole object: any other bit, to show it plays no part
constexpr uint32_t k_ZSorted = 0x200;
} // namespace

// The original prints a placed mesh's footprint when its header has the landscape feature bit and the feature holds
// at least one footprint (a field's 3D object always asks for its print as it is made)
TEST(FootprintPrint, LandscapeFeatureWithAFootprintPrints)
{
	EXPECT_TRUE(MeshPrintsFootprint(k_LandscapeFeature, 1));
	EXPECT_TRUE(MeshPrintsFootprint(k_LandscapeFeature, 3));
	EXPECT_TRUE(MeshPrintsFootprint(k_LandscapeFeature | k_ZSorted, 1));
}

// A landscape feature that holds no footprint prints nothing
TEST(FootprintPrint, FeatureWithoutFootprintsPrintsNothing)
{
	EXPECT_FALSE(MeshPrintsFootprint(k_LandscapeFeature, 0));
}

// Without the header bit the feature is not read, whatever else the mesh has
TEST(FootprintPrint, NoLandscapeFeatureBitPrintsNothing)
{
	EXPECT_FALSE(MeshPrintsFootprint(0, 1));
	EXPECT_FALSE(MeshPrintsFootprint(~k_LandscapeFeature, 2));
}

// It is a compile-time rule
static_assert(MeshPrintsFootprint(k_LandscapeFeature, 1));
static_assert(!MeshPrintsFootprint(k_ZSorted, 1));
