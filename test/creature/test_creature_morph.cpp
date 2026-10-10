/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <bit>
#include <limits>

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureMorph.h"

using namespace openblack;
using A = creature::CreatureBody::Appearance;

TEST(CreatureMorph, AlignmentIsTheEvilGoodAxis)
{
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(-0.4f, 0.5f, 0.5f, 0.5f).evilGood, -0.4f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(3.0f, 0.5f, 0.5f, 0.5f).evilGood, 1.0f);
}

TEST(CreatureMorph, FatnessRunsFromThinToFat)
{
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.0f, 0.5f, 0.5f).thinFat, -1.0f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 0.5f, 0.5f).thinFat, 0.0f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.75f, 0.5f, 0.5f).thinFat, 0.5f);
}

TEST(CreatureMorph, StrengthCountsFourTimesTheSpecies)
{
	// 0.2 of the species' strength and 0.8 of the creature's
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 1.0f, 0.0f).weakStrong, 0.6f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 0.0f, 1.0f).weakStrong, -0.6f);
	EXPECT_FLOAT_EQ(creature_morph::FromAttributes(0.0f, 0.5f, 1.0f, 1.0f).weakStrong, 1.0f);
}

TEST(CreatureMorph, SizeIsKeptBetweenItsLimits)
{
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(0.0f), 0.05f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(1.5f), 1.5f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(9.0f), 4.0f);
}

TEST(CreatureMorph, ASizeThatIsNotANumberIsKeptAtTheLeast)
{
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(std::numeric_limits<float>::quiet_NaN()), 0.05f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(0.05f), 0.05f);
	EXPECT_FLOAT_EQ(creature_morph::ClampScale(4.0f), 4.0f);
}

TEST(CreatureMorph, EachAxisPullsTowardsTheMeshOnItsSide)
{
	EXPECT_EQ(creature_morph::EvilGoodMesh(-0.1f), A::Evil);
	EXPECT_EQ(creature_morph::EvilGoodMesh(0.0f), A::Good);
	EXPECT_EQ(creature_morph::ThinFatMesh(-1.0f), A::Thin);
	EXPECT_EQ(creature_morph::ThinFatMesh(0.3f), A::Fat);
	EXPECT_EQ(creature_morph::WeakStrongMesh(-0.5f), A::Weak);
	EXPECT_EQ(creature_morph::WeakStrongMesh(1.0f), A::Strong);
}

TEST(CreatureMorph, BlendMovesTheBaseTowardsEachMesh)
{
	const glm::vec3 base {0.0f, 0.0f, 0.0f};
	const glm::vec3 evilGood {1.0f, 0.0f, 0.0f};
	const glm::vec3 thinFat {0.0f, 2.0f, 0.0f};
	const glm::vec3 weakStrong {0.0f, 0.0f, 4.0f};
	const auto blended =
	    creature_morph::Blend(base, evilGood, thinFat, weakStrong, {.evilGood = -0.5f, .thinFat = 0.25f, .weakStrong = 1.0f});
	EXPECT_FLOAT_EQ(blended.x, 0.5f);
	EXPECT_FLOAT_EQ(blended.y, 0.5f);
	EXPECT_FLOAT_EQ(blended.z, 4.0f);
	EXPECT_EQ(creature_morph::Blend(base, evilGood, thinFat, weakStrong, {}), base);
}

TEST(CreatureMorph, ShownFatnessFollowsByAHundredthATurn)
{
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 1.0f), 0.51f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 0.0f), 0.49f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.5f, 0.505f), 0.505f);
}

TEST(CreatureMorph, ShownFatnessStaysWithinNoneAndFull)
{
	// a fatness out of 0 to 1, as a script may set it, is shown at most full and at least none
	EXPECT_EQ(creature_morph::EaseFatness(0.995f, 1.5f), 1.0f);
	EXPECT_EQ(creature_morph::EaseFatness(1.0f, 3.0f), 1.0f);
	EXPECT_EQ(creature_morph::EaseFatness(0.005f, -0.5f), 0.0f);
	EXPECT_EQ(creature_morph::EaseFatness(0.0f, -1.0f), 0.0f);
	// a shown fatness already out of 0 to 1 is brought back at once, not by a hundredth
	EXPECT_EQ(creature_morph::EaseFatness(1.2f, 1.2f), 1.0f);
	EXPECT_EQ(creature_morph::EaseFatness(-0.3f, -0.3f), 0.0f);
	// within, nothing changes
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.99f, 1.0f), 1.0f);
	EXPECT_FLOAT_EQ(creature_morph::EaseFatness(0.01f, 0.0f), 0.0f);
}

TEST(CreatureMorph, SmallChangesDoNotRedrawTheBody)
{
	const creature_morph::Morph drawn {.evilGood = 0.1f, .thinFat = 0.2f, .weakStrong = 0.3f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.12f, .thinFat = 0.22f, .weakStrong = 0.32f});
	EXPECT_FALSE(refresh.animations);
	EXPECT_FALSE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.1f);
}

TEST(CreatureMorph, ANewAlignmentRedrawsEveryAxis)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.5f, .thinFat = 0.01f, .weakStrong = 0.02f});
	EXPECT_TRUE(refresh.animations);
	EXPECT_TRUE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.5f);
	EXPECT_FLOAT_EQ(refresh.drawn.thinFat, 0.01f);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, 0.02f);
}

TEST(CreatureMorph, ANewFatnessRedrawsFatnessAndStrength)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.02f, .thinFat = -0.03f, .weakStrong = 0.01f});
	EXPECT_TRUE(refresh.animations);
	EXPECT_FLOAT_EQ(refresh.drawn.evilGood, 0.0f);
	EXPECT_FLOAT_EQ(refresh.drawn.thinFat, -0.03f);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, 0.01f);
}

TEST(CreatureMorph, ANewStrengthOnlyReshapesTheVertices)
{
	const creature_morph::Morph drawn {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = 0.0f};
	const auto refresh = creature_morph::RefreshDrawn(drawn, {.evilGood = 0.0f, .thinFat = 0.0f, .weakStrong = -1.0f});
	EXPECT_FALSE(refresh.animations);
	EXPECT_TRUE(refresh.vertices);
	EXPECT_FLOAT_EQ(refresh.drawn.weakStrong, -1.0f);
}

TEST(CreatureMorph, MissingMeshesFallBackOnTheBase)
{
	const auto base = creature::GetIdFromType(CreatureType::Tiger, A::Base);
	const auto evil = creature::GetIdFromType(CreatureType::Tiger, A::Evil);
	const auto meshes = creature_morph::MeshesOf(CreatureType::Tiger, {.evilGood = -0.5f, .thinFat = 0.5f},
	                                             [evil](entt::id_type id) { return id == evil; });
	EXPECT_EQ(meshes.base, base);
	EXPECT_EQ(meshes.evilGood, evil);
	EXPECT_EQ(meshes.thinFat, base);
	EXPECT_EQ(meshes.weakStrong, base);
}

TEST(CreatureMorph, ACreatureOfSizeOneIsFifteenUnitsTall)
{
	std::array<glm::mat4, 2> rest {glm::mat4(1.0f), glm::mat4(1.0f)};
	rest[0][3].y = 40.0f;
	rest[1][3].y = -20.0f;
	EXPECT_FLOAT_EQ(creature_morph::RestHeight(rest), 60.0f);
	EXPECT_FLOAT_EQ(creature_morph::DrawnScale(2.0f, 60.0f), 0.5f);
	// Bones all above the origin still reach down to it
	rest[1][3].y = 10.0f;
	EXPECT_FLOAT_EQ(creature_morph::RestHeight(rest), 40.0f);
	EXPECT_FLOAT_EQ(creature_morph::DrawnScale(2.0f, 0.0f), 2.0f);
}

TEST(CreatureMorph, TheReachIsTheFurthestBoneFromTheUprightAxis)
{
	// y does not count: the bone 20 below reaches only 1 out
	const std::array<glm::vec3, 3> bones {glm::vec3(3.0f, 9.0f, 4.0f), glm::vec3(-6.0f, 1.0f, 0.0f),
	                                      glm::vec3(0.0f, -20.0f, 1.0f)};
	EXPECT_EQ(creature_morph::Reach(bones), 6.0f);
	// the first bone the furthest
	const std::array<glm::vec3, 2> first {glm::vec3(0.0f, 0.0f, -8.0f), glm::vec3(1.0f, 0.0f, 1.0f)};
	EXPECT_EQ(creature_morph::Reach(first), 8.0f);
	// nothing reaches 0, and a bone that is not a number is never kept
	EXPECT_EQ(creature_morph::Reach({}), 0.0f);
	const std::array<glm::vec3, 2> nan {glm::vec3(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f),
	                                    glm::vec3(2.0f, 0.0f, 0.0f)};
	EXPECT_EQ(creature_morph::Reach(nan), 2.0f);
}

TEST(CreatureMorph, TheRadiusIsTheDrawnScaleTimesTheReach)
{
	// 15 x 16 / 48
	EXPECT_EQ(creature_morph::Radius(1.0f, 48.0f, 16.0f), 5.0f);
	// the size kept between its limits first, a size that is not a number at the least
	EXPECT_EQ(creature_morph::Radius(0.01f, 48.0f, 16.0f), creature_morph::Radius(0.05f, 48.0f, 16.0f));
	EXPECT_EQ(creature_morph::Radius(9.0f, 48.0f, 16.0f), creature_morph::Radius(4.0f, 48.0f, 16.0f));
	EXPECT_EQ(creature_morph::Radius(std::numeric_limits<float>::quiet_NaN(), 48.0f, 16.0f),
	          creature_morph::Radius(0.05f, 48.0f, 16.0f));
	// in a pen, at its size there: ((0.22 x 15) / 48) x 16, each step a float
	EXPECT_EQ(std::bit_cast<uint32_t>(creature_morph::Radius(0.22f, 48.0f, 16.0f)), 0x3F8CCCCDu);
	// without a rest height the size itself is the scale (DrawnScale's, where the game would divide by 0)
	EXPECT_EQ(creature_morph::Radius(2.0f, 0.0f, 3.0f), 6.0f);
}
