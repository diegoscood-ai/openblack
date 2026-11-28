/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a creature holds its hands to cast, on a made-up skeleton: halfway between its fingertips' mean and its mirror
// hand's, from the body as posed for the turn (the kept pose while there is one, never the drawn one), the beams at the
// two hand bones themselves, and the way from the hands as the game makes it one long

#include <cmath>
#include <cstdint>

#include <limits>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureHands.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Registry.h"

using namespace openblack;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureTurnPose;

namespace
{
constexpr uint32_t k_NoParent = std::numeric_limits<uint32_t>::max();
constexpr uint32_t k_Hand = 2;
constexpr uint32_t k_Mirror = 7;

glm::mat4 At(float x, float y, float z)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

/// Root, an arm and its hand with three fingertips; the other arm, its hand and three fingertips
const std::vector<uint32_t> k_Parents {k_NoParent, 0, 1, 2, 2, 2, 0, 6, 7, 7, 7};

/// The body with its right fingertips about x = 10 and its left about x = -10
std::vector<glm::mat4> Body(float lift)
{
	return {At(0, 0, 0),          At(5, 20, 0),          At(8, 20, 0),         At(9, 21 + lift, 0),
	        At(10, 22 + lift, 1), At(11, 23 + lift, 2),  At(-5, 20, 0),        At(-8, 20, 0),
	        At(-9, 21 + lift, 0), At(-10, 22 + lift, 1), At(-11, 23 + lift, 2)};
}

/// A rig whose hand is bone 2, its file's table pairing 2 with 7 and every other bone with itself
creature::CreatureRig Rig()
{
	creature::CreatureRig rig;
	rig.actionPoints = creature::CreatureRig::ActionPoints {.rightHand = k_Hand};
	// a number before the bones' table, then the table
	rig.fileTail = {42, 0, 1, 7, 3, 4, 5, 6, 2, 8, 9, 10};
	return rig;
}
} // namespace

TEST(CreatureHands, HalfwayBetweenTheFingertipsOfBothHands)
{
	const auto bones = Body(0.0f);
	const auto hands = creature_hands::HandPoint(k_Parents, bones, k_Hand, k_Mirror);
	ASSERT_TRUE(hands.has_value());
	// each hand's tips' mean, (9 + 10 + 11) / 3 and the mirror's, then halved: the sums of both sides cancel in x
	const float third = 1.0f / 3.0f;
	const glm::vec3 right(third * 30.0f, third * 66.0f, third * 3.0f);
	const glm::vec3 left(third * -30.0f, third * 66.0f, third * 3.0f);
	EXPECT_EQ(*hands, glm::vec3((right.x + left.x) * 0.5f, (right.y + left.y) * 0.5f, (right.z + left.z) * 0.5f));
	// not the hand bones' own places
	EXPECT_NE(hands->y, 20.0f);
	// none for a bone the skeleton does not have
	EXPECT_FALSE(creature_hands::HandPoint(k_Parents, bones, k_Hand, 11).has_value());
}

TEST(CreatureHands, TheHandBoneAndItsMirrorFromTheRig)
{
	const auto rig = Rig();
	const auto bones = creature_hands::HandBones(rig, k_Parents.size());
	ASSERT_TRUE(bones.has_value());
	EXPECT_EQ(bones->first, k_Hand);
	EXPECT_EQ(bones->second, k_Mirror);
	// a table shorter than the body, or a species with no action points
	EXPECT_FALSE(creature_hands::HandBones(rig, 20).has_value());
	creature::CreatureRig none;
	EXPECT_FALSE(creature_hands::HandBones(none, k_Parents.size()).has_value());
}

TEST(CreatureHands, TheWayFromTheHandsIsMadeOneLong)
{
	const auto way = creature_hands::Towards(glm::vec3(3.0f, 14.0f, 4.0f), glm::vec3(0.0f, 14.0f, 0.0f));
	const float inverse = 1.0f / std::sqrt(((4.0f * 4.0f) + (0.0f * 0.0f)) + (3.0f * 3.0f));
	EXPECT_EQ(way, glm::vec3(3.0f * inverse, 0.0f, 4.0f * inverse));
	// none at all is left as it is
	EXPECT_EQ(creature_hands::Towards(glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(1.0f, 2.0f, 3.0f)), glm::vec3(0.0f));
	// and a part that is not a number counts as none
	const auto nan = std::numeric_limits<float>::quiet_NaN();
	const auto odd = creature_hands::Towards(glm::vec3(nan, 0.0f, 0.0f), glm::vec3(0.0f));
	EXPECT_TRUE(std::isnan(odd.x));
	EXPECT_EQ(odd.y, 0.0f);
	EXPECT_EQ(odd.z, 0.0f);
}

TEST(CreatureHands, ACreaturesHandsAreReadFromTheTurnsPoseOrTheKeptOne)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	const auto rig = Rig();
	// before it is posed there are none
	registry.Assign<CreatureAnimation>(creature).skeleton.parents = k_Parents;
	EXPECT_FALSE(creature_hands::HandsOf(registry, creature, rig).has_value());
	// the frame's own pose is never read
	registry.Get<CreatureAnimation>(creature).boneMatrices = Body(100.0f);
	auto& pose = registry.Assign<CreatureTurnPose>(creature);
	EXPECT_FALSE(creature_hands::HandsOf(registry, creature, rig).has_value());
	pose.current = Body(0.0f);
	const auto posed = creature_hands::HandsOf(registry, creature, rig);
	ASSERT_TRUE(posed.has_value());
	EXPECT_EQ(*posed, creature_hands::HandPoint(k_Parents, pose.current, k_Hand, k_Mirror));
	// while the hand holds it, the pose kept then
	pose.frozen = Body(6.0f);
	const auto kept = creature_hands::HandsOf(registry, creature, rig);
	ASSERT_TRUE(kept.has_value());
	EXPECT_EQ(*kept, creature_hands::HandPoint(k_Parents, *pose.frozen, k_Hand, k_Mirror));
	EXPECT_NE(*kept, *posed);
}

TEST(CreatureHands, TheBeamsStartAtTheMirrorAndTheHandBones)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	registry.Assign<CreatureAnimation>(creature).skeleton.parents = k_Parents;
	registry.Assign<CreatureTurnPose>(creature).current = Body(0.0f);
	const auto points = creature_hands::BeamPointsOf(registry, creature, Rig());
	ASSERT_TRUE(points.has_value());
	EXPECT_EQ(points->first, glm::vec3(-8.0f, 20.0f, 0.0f));
	EXPECT_EQ(points->second, glm::vec3(8.0f, 20.0f, 0.0f));
	// none for a creature that has gone
	registry.Destroy(creature);
	EXPECT_FALSE(creature_hands::BeamPointsOf(registry, creature, Rig()).has_value());
}
