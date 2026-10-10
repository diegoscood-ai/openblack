/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature's body posed for the turn, on made-up bones: a new pose and the one before, the copy kept in the hand,
// the turn's animation step, the drawn blend, the file's mirror table and the mean of the bones at a branch's ends

#include <cstdint>

#include <limits>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/CreatureTurnPose.h"

using namespace openblack;
using openblack::ecs::components::CreatureTurnPose;

namespace
{
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();

glm::mat4 At(float x, float y, float z)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

/// Three bones all at one place, which tells the poses apart
std::vector<glm::mat4> Pose(float x)
{
	return {At(x, 0.0f, 0.0f), At(x, 1.0f, 0.0f), At(x, 2.0f, 0.0f)};
}
} // namespace

TEST(CreatureTurnPoseStep, TheTurnTimesTheRateOfTheSizeCutTowardsZero)
{
	// 1.6 - (1 x 0.5) x 0.85 = 1.175, x 100 = 117.5 in floats, cut to 117 rather than rounded to 118
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, false, false), 117);
	EXPECT_EQ(creature_turn_pose::Step(100, 0.5f, false, false), 138); // 138.75
	EXPECT_EQ(creature_turn_pose::Step(100, 2.0f, false, false), 75);
	EXPECT_EQ(creature_turn_pose::Step(100, 3.0f, false, false), 32); // 32.499992 in floats
	// past a size of about 3.76 the step goes back, still cut towards zero: -3.3 is -3
	EXPECT_EQ(creature_turn_pose::Step(33, 4.0f, false, false), -3);
}

TEST(CreatureTurnPoseStep, AtTheRealSpeedTheTurnsOwnAndDoubledTwice)
{
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, true, false), 100);
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, false, true), 234);
	EXPECT_EQ(creature_turn_pose::Step(100, 1.0f, true, true), 200);
}

TEST(CreatureTurnPoseAdvance, NoPoseYetThenTheFirstIsAlsoThePrevious)
{
	CreatureTurnPose pose;
	EXPECT_FALSE(creature_turn_pose::HasPose(pose));
	EXPECT_TRUE(creature_turn_pose::Safe(pose).empty());
	EXPECT_TRUE(creature_turn_pose::Drawn(pose, 0.5f).empty());

	creature_turn_pose::Advance(pose, Pose(1.0f));
	EXPECT_TRUE(creature_turn_pose::HasPose(pose));
	EXPECT_EQ(pose.current, Pose(1.0f));
	EXPECT_EQ(pose.previous, Pose(1.0f));
}

TEST(CreatureTurnPoseAdvance, EachPoseMovesTheCurrentToThePrevious)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(1.0f));
	creature_turn_pose::Advance(pose, Pose(2.0f));
	EXPECT_EQ(pose.previous, Pose(1.0f));
	EXPECT_EQ(pose.current, Pose(2.0f));
	// a second pose in the same turn moves it on again
	creature_turn_pose::Advance(pose, Pose(3.0f));
	EXPECT_EQ(pose.previous, Pose(2.0f));
	EXPECT_EQ(pose.current, Pose(3.0f));
}

TEST(CreatureTurnPoseSafe, TheKeptPoseWhileHeldThenTheCurrent)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(1.0f));
	EXPECT_EQ(creature_turn_pose::Safe(pose).data(), pose.current.data());

	// held before the turn's pose: the game keeps reading the turn before's while the current moves on
	creature_turn_pose::Freeze(pose);
	creature_turn_pose::Advance(pose, Pose(2.0f));
	const auto held = creature_turn_pose::Safe(pose);
	EXPECT_EQ(std::vector<glm::mat4>(held.begin(), held.end()), Pose(1.0f));
	EXPECT_EQ(pose.current, Pose(2.0f));

	// taken again by the hand, it keeps the current one
	creature_turn_pose::Freeze(pose);
	const auto again = creature_turn_pose::Safe(pose);
	EXPECT_EQ(std::vector<glm::mat4>(again.begin(), again.end()), Pose(2.0f));

	creature_turn_pose::Reconnect(pose);
	EXPECT_FALSE(pose.frozen.has_value());
	EXPECT_EQ(creature_turn_pose::Safe(pose).data(), pose.current.data());
}

TEST(CreatureTurnPoseBlend, TheFirstAssignsTheNextAdd)
{
	std::vector<glm::mat4> dst(2, glm::mat4(5.0f));
	std::vector<glm::mat4> src(2, glm::mat4(2.0f));
	src[1][3] = glm::vec4(4.0f, 8.0f, 12.0f, 2.0f);
	creature_turn_pose::BlendInto(dst, src, 0.25f, true);
	EXPECT_EQ(dst[0][0], glm::vec4(0.5f, 0.0f, 0.0f, 0.0f));
	creature_turn_pose::BlendInto(dst, src, 0.5f, false);
	EXPECT_EQ(dst[1][2], glm::vec4(0.0f, 0.0f, 1.5f, 0.0f));
	// the place too, not only the diagonal
	EXPECT_EQ(glm::vec3(dst[1][3]), glm::vec3(3.0f, 6.0f, 9.0f));
	// the bottom row is not blended
	EXPECT_EQ(dst[1][3][3], 1.0f);
	EXPECT_EQ(dst[1][0][3], 0.0f);
}

TEST(CreatureTurnPoseBlend, OnlyTheBonesBothHave)
{
	std::vector<glm::mat4> dst(3, glm::mat4(1.0f));
	const std::vector<glm::mat4> src(2, glm::mat4(2.0f));
	creature_turn_pose::BlendInto(dst, src, 1.0f, true);
	// blended but for the bottom row, which stays that of an affine matrix
	auto blended = glm::mat4(2.0f);
	blended[3][3] = 1.0f;
	EXPECT_EQ(dst[1], blended);
	EXPECT_EQ(dst[2], glm::mat4(1.0f));
}

TEST(CreatureTurnPoseDrawn, BetweenThePreviousAndTheCurrent)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(2.0f));
	creature_turn_pose::Advance(pose, Pose(4.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 0.0f), Pose(2.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 1.0f), Pose(4.0f));
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 0.5f), Pose(3.0f));
}

TEST(CreatureTurnPoseDrawn, PastTheTurnItGoesOnUpToTwice)
{
	CreatureTurnPose pose;
	creature_turn_pose::Advance(pose, Pose(2.0f));
	creature_turn_pose::Advance(pose, Pose(4.0f));
	// 1.5 x 4 + (1 - 1.5) x 2
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 1.5f), Pose(5.0f));
	// taken as 2: 2 x 4 - 2
	EXPECT_EQ(creature_turn_pose::Drawn(pose, 3.0f), Pose(6.0f));
}

TEST(CreatureTurnPoseWorld, TheBodysMatrixBeforeEachBone)
{
	const auto world = At(10.0f, 0.0f, 0.0f) * glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
	const auto placed = creature_turn_pose::InWorld(Pose(1.0f), world);
	ASSERT_EQ(placed.size(), 3u);
	EXPECT_EQ(glm::vec3(placed[2][3]), glm::vec3(12.0f, 4.0f, 0.0f));
	EXPECT_TRUE(creature_turn_pose::InWorld({}, world).empty());
}

TEST(CreatureTurnPoseMirror, TheFilesLastNumberForEachBone)
{
	// two numbers before the bones' table, then bone 0 <-> 2 and bone 1 its own
	const std::vector<int32_t> tail {7, 9, 2, 1, 0};
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 0), 2u);
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 1), 1u);
	EXPECT_EQ(creature_turn_pose::MirrorBone(tail, 3, 2), 0u);
	EXPECT_FALSE(creature_turn_pose::MirrorBone(tail, 3, 3).has_value());
	// a table shorter than the bones, or naming no bone
	EXPECT_FALSE(creature_turn_pose::MirrorBone(tail, 6, 0).has_value());
	const std::vector<int32_t> none {-1, 5, 0};
	EXPECT_FALSE(creature_turn_pose::MirrorBone(none, 3, 0).has_value());
	EXPECT_FALSE(creature_turn_pose::MirrorBone(none, 3, 1).has_value());
}

TEST(CreatureTurnPoseLeafMean, TheFingertipsUnderTheHand)
{
	// root, arm, hand; under the hand a finger of two bones (3, then its tip 6) and two tips (4, 5)
	const std::vector<uint32_t> parents {k_NoParent, 0, 1, 2, 2, 2, 3};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1, 0, 0), At(2, 0, 0), At(3, 0, 0),
	                                    At(4, 1, 0), At(5, 2, 0), At(9, 3, 0)};
	const auto mean = creature_turn_pose::LeafMean(parents, bones, 2);
	ASSERT_TRUE(mean.has_value());
	// tips 6, 4 and 5: (9 + 4 + 5, 3 + 1 + 2) x 1/3, the finger's base 3 not counted
	const float third = 1.0f / 3.0f;
	EXPECT_EQ(*mean, glm::vec3(third * 18.0f, third * 6.0f, 0.0f));
	// a tip is its own mean
	EXPECT_EQ(creature_turn_pose::LeafMean(parents, bones, 6), glm::vec3(9.0f, 3.0f, 0.0f));
}

TEST(CreatureTurnPoseLeafMean, TheBonesAfterTheStartUnderItsParentCountToo)
{
	// the walk goes on from the start bone along the bones after it under the same parent: from tip 4, tip 5 as well
	const std::vector<uint32_t> parents {k_NoParent, 0, 1, 2, 2, 2, 3};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1, 0, 0), At(2, 0, 0), At(3, 0, 0),
	                                    At(4, 1, 0), At(6, 2, 0), At(9, 3, 0)};
	EXPECT_EQ(creature_turn_pose::LeafMean(parents, bones, 4), glm::vec3(0.5f * 10.0f, 0.5f * 3.0f, 0.0f));
}

TEST(CreatureTurnPoseLeafMean, TheSumIsRoundedAfterEachAdd)
{
	// 1e8 + 4 is 1e8 in floats, and + 4 again still 1e8; as (4 + 4) + 1e8 it would be 1e8 + 8
	const std::vector<uint32_t> parents {k_NoParent, 0, 0, 0};
	const std::vector<glm::mat4> bones {At(0, 0, 0), At(1e8f, 0, 0), At(4, 0, 0), At(4, 0, 0)};
	const auto mean = creature_turn_pose::LeafMean(parents, bones, 0);
	ASSERT_TRUE(mean.has_value());
	EXPECT_EQ(mean->x, (1.0f / 3.0f) * 1e8f);
	EXPECT_NE(mean->x, (1.0f / 3.0f) * (1e8f + 8.0f));
}

TEST(CreatureTurnPoseLeafMean, NoneWithoutTheBones)
{
	const std::vector<uint32_t> parents {k_NoParent, 0};
	EXPECT_FALSE(creature_turn_pose::LeafMean(parents, std::vector<glm::mat4>(2), 2).has_value());
	EXPECT_FALSE(creature_turn_pose::LeafMean(parents, std::vector<glm::mat4>(1), 0).has_value());
}
