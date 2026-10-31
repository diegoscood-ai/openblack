/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The samples the player's hand plays and how (hand_sounds), docs/wiki/project/progress/hand/hand_sounds.md and
// docs/wiki/audio/magic-and-weather-sounds.md

#include <gtest/gtest.h>

#include "Hand/HandSoundRules.h"

using namespace openblack;
namespace hs = openblack::hand_sounds;

TEST(HandSounds, GrippingTheLandPicksOneOfSixLandGrabSamples)
{
	// the in-game bank's samples 4 to 9
	EXPECT_EQ(hs::k_GripLandFirst, 4);
	EXPECT_EQ(hs::k_GripLandCount, 6);
}

TEST(HandSounds, TheLandGrabSoundIsFlat)
{
	EXPECT_FALSE(hs::k_GripLandIs3D);
}

TEST(HandSounds, GrippingTheSeaPlaysTheTenWaterSamplesByTheCounter)
{
	// samples 99 to 108, the counter's value added to the first
	EXPECT_EQ(hs::WaterSample(0), 99);
	EXPECT_EQ(hs::WaterSample(5), 104);
	EXPECT_EQ(hs::WaterSample(9), 108);
}

TEST(HandSounds, TheWaterSoundIsPlacedOnTheSurfaceWhereTheHandWentIn)
{
	EXPECT_TRUE(hs::k_WaterIs3D);
	const auto point = hs::WaterPoint(glm::vec3(1234.5f, -7.0f, 987.25f));
	EXPECT_EQ(point.x, 1234.5f);
	EXPECT_EQ(point.y, 0.2f);
	EXPECT_EQ(point.z, 987.25f);
}

TEST(HandSounds, PickingUpPlaysThePickUpSampleButNotForARootedTreeOrForest)
{
	EXPECT_EQ(hs::k_PickUpObject, 10);
	EXPECT_TRUE(hs::k_PickUpIs3D);
	EXPECT_TRUE(hs::PlaysPickUpSample(false, false));
	EXPECT_TRUE(hs::PlaysPickUpSample(false, true));
	// a tree or forest in the ground is silent; one flying in the physics (caught) is not
	EXPECT_FALSE(hs::PlaysPickUpSample(true, false));
	EXPECT_TRUE(hs::PlaysPickUpSample(true, true));
}

TEST(HandSounds, AVillagerPickedUpScreamsAsAChildAWomanOrAMan)
{
	EXPECT_EQ(hs::k_ScreamCount, 7);
	EXPECT_EQ(hs::ScreamFirst(true, false), 180);
	// a child first, whatever else
	EXPECT_EQ(hs::ScreamFirst(true, true), 180);
	EXPECT_EQ(hs::ScreamFirst(false, true), 194);
	EXPECT_EQ(hs::ScreamFirst(false, false), 187);
}

TEST(HandSounds, ATreeReplantedPlaysOneOfThreePlantingSamplesByTheTickCount)
{
	EXPECT_EQ(hs::PlantTreeSample(0u), 83);
	EXPECT_EQ(hs::PlantTreeSample(1u), 84);
	EXPECT_EQ(hs::PlantTreeSample(2u), 85);
	EXPECT_EQ(hs::PlantTreeSample(3u), 83);
	// the tick count wraps at 2^32: its remainder is taken unsigned
	EXPECT_EQ(hs::PlantTreeSample(0xFFFFFFFFu), 83);
	EXPECT_EQ(hs::PlantTreeSample(0xFFFFFFFEu), 85);
}

TEST(HandSounds, ATreeUprootedPlaysOneOfThreeTreeBreakSamples)
{
	EXPECT_EQ(hs::k_TreeBreakFirst, 32);
	EXPECT_EQ(hs::k_TreeBreakCount, 3);
}
