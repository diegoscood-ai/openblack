/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Where a creature's groin and belly are, as the scripts' arse and belly positions read them, on a made-up skeleton:
// the species' groin and belly bones in the body as posed for the turn (the kept pose while the hand holds it), never
// the frame's own pose; none before it is posed, for a bone the pose lacks or for a species without action points

#include <cstdint>

#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "Creature/CreatureRig.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/CreaturePose.h"
#include "ECS/Registry.h"

using namespace openblack;
using openblack::ecs::components::CreatureAnimation;
using openblack::ecs::components::CreatureTurnPose;

namespace
{
constexpr uint32_t k_Belly = 2;
constexpr uint32_t k_Groin = 4;

glm::mat4 At(float x, float y, float z)
{
	return glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
}

/// Five bones, each placed apart, lifted by `lift`
std::vector<glm::mat4> Body(float lift)
{
	return {At(0, lift, 0), At(1, 10 + lift, 2), At(3, 12 + lift, 4), At(5, 14 + lift, 6), At(7, 3 + lift, -8)};
}

/// A rig whose belly is bone 2 and whose groin is bone 4
creature::CreatureRig Rig()
{
	creature::CreatureRig rig;
	rig.actionPoints = creature::CreatureRig::ActionPoints {.belly = k_Belly, .groin = k_Groin};
	return rig;
}
} // namespace

TEST(CreatureBodyPoints, TheGroinAndTheBellyAreReadFromTheTurnsPoseOrTheKeptOne)
{
	ecs::Registry registry;
	const auto body = registry.Create();
	const auto rig = Rig();
	// before it is posed there are none, and the frame's own pose is never read
	registry.Assign<CreatureAnimation>(body).boneMatrices = Body(100.0f);
	EXPECT_FALSE(ecs::creature_pose::GroinOf(registry, body, rig).has_value());
	auto& pose = registry.Assign<CreatureTurnPose>(body);
	EXPECT_FALSE(ecs::creature_pose::GroinOf(registry, body, rig).has_value());
	EXPECT_FALSE(ecs::creature_pose::BellyOf(registry, body, rig).has_value());
	// the turn's pose: each bone's own place
	pose.current = Body(0.0f);
	EXPECT_EQ(ecs::creature_pose::GroinOf(registry, body, rig), glm::vec3(7.0f, 3.0f, -8.0f));
	EXPECT_EQ(ecs::creature_pose::BellyOf(registry, body, rig), glm::vec3(3.0f, 12.0f, 4.0f));
	// while the hand holds it, the pose kept then
	pose.frozen = Body(6.0f);
	EXPECT_EQ(ecs::creature_pose::GroinOf(registry, body, rig), glm::vec3(7.0f, 9.0f, -8.0f));
	EXPECT_EQ(ecs::creature_pose::BellyOf(registry, body, rig), glm::vec3(3.0f, 18.0f, 4.0f));
}

TEST(CreatureBodyPoints, NoneForABoneThePoseLacksASpeciesWithoutPointsOrACreatureGone)
{
	ecs::Registry registry;
	const auto body = registry.Create();
	registry.Assign<CreatureTurnPose>(body).current = Body(0.0f);
	// a pose shorter than the groin's bone still has the belly's
	registry.Get<CreatureTurnPose>(body).current.resize(k_Groin);
	EXPECT_FALSE(ecs::creature_pose::GroinOf(registry, body, Rig()).has_value());
	EXPECT_EQ(ecs::creature_pose::BellyOf(registry, body, Rig()), glm::vec3(3.0f, 12.0f, 4.0f));
	// a species whose file has no action points
	EXPECT_FALSE(ecs::creature_pose::BellyOf(registry, body, creature::CreatureRig {}).has_value());
	registry.Destroy(body);
	EXPECT_FALSE(ecs::creature_pose::BellyOf(registry, body, Rig()).has_value());
}
