/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The creature animation system on a registry of its own: the breathing eases once a turn (the fatness shown is eased by
// the body, after the creature's actions), the eyes blink by the turn on the C runtime's numbers, nothing is posed
// without a mesh or a rig, and the body's pose is put in the world for the turn at the turn's end, sampled as it plays then
// without moving anything on. The frame's pose is its step of the layers, then the sample of what the step leaves.

#define LOCATOR_IMPLEMENTATIONS

#include <cmath>

#include <chrono>
#include <utility>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "3D/ObjectMatrix.h"
#include "3D/SkeletalAnimation.h"
#include "Common/GameRandom.h"
#include "Common/GameRandomTesting.h"
#include "Creature/CreatureAnimation.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureTurnPose.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreaturePose.h"
#include "ECS/Systems/Implementations/CreatureAnimationSystem.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"
#include "creature/CreatureSystemWorld.h"
#include "creature/SyntheticCreatureBlock.h"
#include "support/TestServices.h"

using namespace openblack;
using namespace openblack::ecs::components;
using openblack::ecs::systems::CreatureAnimationSystem;

namespace
{
class CreatureAnimationSystemTest: public ::testing::Test
{
protected:
	test::creature_world::World _world;
	CreatureAnimationSystem _system;
};

/// The ape with a stand and a walk that turns its first bone over a second
class CreatureAnimationSystemRigTest: public ::testing::Test
{
protected:
	void SetUp() override
	{
		test::creature_block::Block block;
		block.clips = {
		    test::creature_block::Clip {},
		    test::creature_block::Clip {.durationMs = 1000, .frames = {{glm::vec3(0.0f)}, {glm::vec3(0.0f, 1.0f, 0.0f)}}}};
		_world.LoadApeRig(block, {{"move", {"Cstand", "Wwalk"}}});
	}

	const test::ScopedDefaultFileSystem _fileSystem;
	test::creature_world::World _world;
	CreatureAnimationSystem _system;
};

/// A rest pose of two bones, the second a metre above the first
const std::vector<glm::mat4> k_TwoBoneRest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 1.0f, 0.0f))};

/// A two bone body at its rest pose, a little way into a breath
CreatureAnimation TwoBoneBody()
{
	CreatureAnimation animation;
	const std::vector<uint32_t> parents {skeletal_animation::k_NoParent, 0};
	animation.skeleton = skeletal_animation::Skeleton::FromRestMatrices(parents, k_TwoBoneRest);
	animation.mirror = skeletal_animation::MirrorJoints(k_TwoBoneRest);
	animation.boneMatrices = k_TwoBoneRest;
	animation.breathPeriod = 5.0f;
	animation.breathPhase = 0.3f;
	return animation;
}

/// A rig of the base mesh alone: a stand that rocks the first bone, and a walk that turns the second over 600 ms
creature::CreatureRig TwoBoneRig()
{
	using skeletal_animation::Animation;
	creature::CreatureRig rig;
	const Animation stand {.duration = 1000,
	                       .looping = true,
	                       .rotatedJoints = {0},
	                       .translatedJoints = {},
	                       .frames = {{.eulerAngles = {glm::vec3(0.0f)}, .translations = {}},
	                                  {.eulerAngles = {glm::vec3(0.2f, 0.0f, 0.0f)}, .translations = {}}}};
	const Animation walk {.duration = 600,
	                      .looping = true,
	                      .rotatedJoints = {1},
	                      .translatedJoints = {},
	                      .frames = {{.eulerAngles = {glm::vec3(0.0f)}, .translations = {}},
	                                 {.eulerAngles = {glm::vec3(0.0f, 0.0f, 0.8f)}, .translations = {}},
	                                 {.eulerAngles = {glm::vec3(0.0f, 0.0f, -0.4f)}, .translations = {}}}};
	rig.animations.front() = {stand, walk};
	return rig;
}

/// The walk, played once from a time
creature_layers::BodyAction Walking(float timeMs)
{
	return {.kind = creature_layers::BodyAction::Kind::Once, .animations = {1, 1, 1}, .timeMs = timeMs};
}

const Transform k_Standing {.position = glm::vec3(10.0f, 0.0f, 20.0f), .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)};
} // namespace

TEST(CreatureAnimationSystemPose, TheFramesPoseIsItsStepThenTheSampleOfWhatTheStepLeaves)
{
	const auto rig = TwoBoneRig();
	for (const bool slotted : {false, true})
	{
		auto framed = TwoBoneBody();
		framed.body = Walking(100.0f);
		if (slotted)
		{
			framed.slots = {{.animation = 1, .timeMs = 250.0f, .weight = 1.0f, .mirrored = false}};
		}
		auto split = framed;

		CreatureAnimationSystem::PoseBody(framed, rig, {}, k_Standing, 1.0f, 1.0f, 33.0f, 0.033f);
		CreatureAnimationSystem::AdvanceLayers(split, rig, {}, k_Standing, 1.0f, 1.0f, 33.0f, 0.033f);

		// the step moves the action on, and the sample of what it leaves is the frame's pose, bit for bit
		EXPECT_GT(split.body.timeMs, 100.0f);
		EXPECT_EQ(split.body.timeMs, framed.body.timeMs);
		EXPECT_EQ(split.yaw.angle, framed.yaw.angle);
		EXPECT_EQ(split.pitch.angle, framed.pitch.angle);
		EXPECT_EQ(CreatureAnimationSystem::SampleBody(split, rig, {}, split.body, split.slots), framed.boneMatrices);
		EXPECT_NE(framed.boneMatrices, k_TwoBoneRest);
	}
}

TEST(CreatureAnimationSystemPose, SamplingMovesNothingOnAndPlaysWhatItIsGiven)
{
	const auto rig = TwoBoneRig();
	auto animation = TwoBoneBody();
	animation.body = Walking(100.0f);
	const std::vector<CreatureAnimation::Slot> walk {{.animation = 1, .timeMs = 250.0f, .weight = 1.0f, .mirrored = false}};

	const auto first = CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, walk);
	const auto again = CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, walk);

	EXPECT_EQ(first, again);
	EXPECT_EQ(animation.body.timeMs, 100.0f);
	EXPECT_EQ(animation.breathPhase, 0.3f);
	EXPECT_TRUE(animation.slots.empty());
	EXPECT_EQ(animation.boneMatrices, k_TwoBoneRest);
	// slots play in place of the body's action, as the frame's do; another time is another pose
	EXPECT_NE(CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, {}), first);
	EXPECT_NE(CreatureAnimationSystem::SampleBody(animation, rig, {}, Walking(400.0f), {}),
	          CreatureAnimationSystem::SampleBody(animation, rig, {}, animation.body, {}));
	// without a stand there is nothing to sample
	auto bare = TwoBoneBody();
	EXPECT_TRUE(CreatureAnimationSystem::SampleBody(bare, creature::CreatureRig {}, {}, bare.body, walk).empty());
}

TEST_F(CreatureAnimationSystemTest, ProcessTurnEasesTheBreathingButNotTheFatnessShown)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).fatness = 1.0f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.breathPeriod = 1.0f;
	const auto shown = registry.Get<CreatureMorph>(creature).shownFatness;
	const auto size = registry.Get<Creature>(creature).size;

	_system.ProcessTurn();

	EXPECT_FLOAT_EQ(registry.Get<CreatureMorph>(creature).shownFatness, shown);
	const auto resting = creature_animation::BreathPeriod(size);
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).breathPeriod,
	                creature_animation::EaseBreathPeriod(1.0f, resting, resting, 0.1f));
}

TEST_F(CreatureAnimationSystemTest, InItsPenItBreathesAtThePenSizesPeriod)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<Creature>(creature).size = 2.0f;
	registry.Get<CreatureDrawPose>(creature).size = 0.22f;
	auto& animation = registry.Get<CreatureAnimation>(creature);
	animation.breathPeriod = 1.0f;

	_system.ProcessTurn();

	// 5 x sqrt(0.22), not its own size's 5 x sqrt(2)
	const auto resting = creature_animation::BreathPeriod(0.22f);
	EXPECT_FLOAT_EQ(resting, 5.0f * std::sqrt(0.22f));
	EXPECT_FLOAT_EQ(registry.Get<CreatureAnimation>(creature).breathPeriod,
	                creature_animation::EaseBreathPeriod(1.0f, resting, resting, 0.1f));
	EXPECT_EQ(registry.Get<Creature>(creature).size, 2.0f);
}

TEST_F(CreatureAnimationSystemTest, TheEyesBlinkByTheTurnOnTheRuntimesNumbers)
{
	const game_random::testing::ScopedState state;
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto seeds = game_random::Current();
	const auto crt = game_random::crt::Seed();

	// the first blink comes after a second: ten turns of 100 ms
	for (int turn = 0; turn < 10; ++turn)
	{
		_system.ProcessTurn();
	}
	EXPECT_EQ(registry.Get<CreatureEyes>(creature).blink.state, creature_eyes::Blink::State::Closing);
	EXPECT_EQ(game_random::crt::Seed(), crt);
	// closed for two turns, opening for two, then open again for a time drawn from the C runtime's numbers
	for (int turn = 0; turn < 4; ++turn)
	{
		_system.ProcessTurn();
	}
	const auto& blink = registry.Get<CreatureEyes>(creature).blink;
	EXPECT_EQ(blink.state, creature_eyes::Blink::State::Open);
	EXPECT_GE(blink.timerMs, blink.intervalMs / 2);
	EXPECT_LT(blink.timerMs, blink.intervalMs + (blink.intervalMs / 2));
	EXPECT_NE(game_random::crt::Seed(), crt);
	// the game's synced and local streams are not drawn from
	EXPECT_EQ(game_random::Current().synced, seeds.synced);
	EXPECT_EQ(game_random::Current().local, seeds.local);
}

TEST_F(CreatureAnimationSystemTest, UpdateWithoutAMeshPosesNothing)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	const auto before = registry.Get<CreatureMorph>(creature).revision;

	_system.Update(std::chrono::duration<float, std::milli>(33.0f));

	const auto& animation = registry.Get<CreatureAnimation>(creature);
	EXPECT_TRUE(animation.skeleton.Empty());
	EXPECT_TRUE(animation.boneMatrices.empty());
	EXPECT_EQ(registry.Get<CreatureMorph>(creature).revision, before);
}

TEST_F(CreatureAnimationSystemTest, TheDrawnShapeTakesTheAttributesOnceATurnNotEachFrame)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& morph = registry.Get<CreatureMorph>(creature);
	const auto drawn = morph.drawn;
	const auto revision = morph.revision;
	// the alignment moves to the other end: the frames draw the shape as it was
	auto& body = registry.Get<Creature>(creature);
	body.alignment = drawn.evilGood > 0.0f ? -1.0f : 1.0f;
	_system.Update(std::chrono::duration<float, std::milli>(33.0f));
	EXPECT_EQ(morph.drawn.evilGood, drawn.evilGood);
	EXPECT_EQ(morph.revision, revision);
	// the turn's pose takes it, and the animations are rebuilt for it
	_system.PoseTurn({});
	EXPECT_EQ(morph.drawn.evilGood, body.alignment);
	EXPECT_EQ(morph.revision, revision + 1);
}

TEST_F(CreatureAnimationSystemTest, AShapeChangeUnderTheThresholdIsDrawnAsItWas)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	_system.PoseTurn({});
	auto& morph = registry.Get<CreatureMorph>(creature);
	const auto drawn = morph.drawn;
	const auto revision = morph.revision;
	// 0.02 is less than the 0.03 a shape axis waits for
	auto& body = registry.Get<Creature>(creature);
	body.alignment = drawn.evilGood > 0.0f ? drawn.evilGood - 0.02f : drawn.evilGood + 0.02f;
	_system.PoseTurn({});
	EXPECT_EQ(morph.drawn.evilGood, drawn.evilGood);
	EXPECT_EQ(morph.revision, revision);
}

TEST_F(CreatureAnimationSystemTest, NoRigMeansNoBoneOrDuration)
{
	const auto creature = test::creature_world::World::MakeCreature();
	EXPECT_FALSE(_system.AnimationDuration(creature, 0).has_value());
	EXPECT_FALSE(_system.BoneInAnimation(creature, 0, 0.0f, 0, false).has_value());
	// nor for something that is no creature
	const auto thing = test::creature_world::World::Registry().Create();
	EXPECT_FALSE(_system.AnimationDuration(thing, 0).has_value());
}

TEST_F(CreatureAnimationSystemTest, ABodyNotPosedYetHasNoTurnPose)
{
	const auto creature = test::creature_world::World::MakeCreature();
	const auto& registry = std::as_const(test::creature_world::World::Registry());
	ASSERT_TRUE(registry.Get<const CreatureAnimation>(creature).boneMatrices.empty());

	_system.PoseTurn({});

	EXPECT_EQ(registry.TryGet<const CreatureTurnPose>(creature), nullptr);
}

TEST_F(CreatureAnimationSystemTest, TheTurnPoseIsTheBodysPoseInTheWorld)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const std::vector<glm::mat4> first {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 2.0f, 0.0f))};
	animation.boneMatrices = first;
	animation.body.timeMs = 250.0f;
	// in its pen, at the scale it is drawn at there
	registry.Get<CreatureDrawPose>(creature).scale = glm::vec3(0.5f);
	const auto& transform = registry.Get<const Transform>(creature);
	const auto world = affine::Model(transform.position, transform.rotation, glm::vec3(0.5f));

	_system.PoseTurn({});

	const auto* pose = std::as_const(registry).TryGet<const CreatureTurnPose>(creature);
	ASSERT_NE(pose, nullptr);
	EXPECT_EQ(pose->current, creature_turn_pose::InWorld(first, world));
	EXPECT_EQ(pose->previous, pose->current);
	EXPECT_FALSE(pose->frozen.has_value());
	// the pose is read, the animation's clock not moved
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).body.timeMs, 250.0f);
	EXPECT_EQ(registry.Get<CreatureAnimation>(creature).boneMatrices, first);
}

TEST_F(CreatureAnimationSystemTest, EachPoseRequestMovesTheLastOneToThePrevious)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	auto& animation = registry.Get<CreatureAnimation>(creature);
	const auto world = affine::Model(registry.Get<const Transform>(creature));
	const std::vector<glm::mat4> first {glm::mat4(1.0f)};
	const std::vector<glm::mat4> second {glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 0.0f, 0.0f))};
	animation.boneMatrices = first;
	_system.PoseTurn({});
	registry.Get<CreatureAnimation>(creature).boneMatrices = second;
	_system.PoseTurn({});

	const auto& pose = registry.Get<CreatureTurnPose>(creature);
	EXPECT_EQ(pose.previous, creature_turn_pose::InWorld(first, world));
	EXPECT_EQ(pose.current, creature_turn_pose::InWorld(second, world));
}

TEST_F(CreatureAnimationSystemTest, TheFrameAndTheTurnLeaveTheTurnPoseAlone)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	registry.Get<CreatureAnimation>(creature).boneMatrices = {glm::mat4(2.0f)};
	_system.PoseTurn({});
	const auto posed = registry.Get<CreatureTurnPose>(creature).current;
	registry.Get<CreatureAnimation>(creature).boneMatrices = {glm::mat4(3.0f)};

	_system.Update(std::chrono::duration<float, std::milli>(33.0f));
	_system.ProcessTurn();

	// a reader before the next pose request still sees the last one
	EXPECT_EQ(registry.Get<CreatureTurnPose>(creature).current, posed);
	EXPECT_EQ(registry.Get<CreatureTurnPose>(creature).previous, posed);
}

TEST_F(CreatureAnimationSystemRigTest, TheTurnPoseIsSampledAsItPlaysAtTheTurnsEndWithoutMovingItOn)
{
	const auto creature = test::creature_world::World::MakeCreature();
	auto& registry = test::creature_world::World::Registry();
	{
		auto& animation = registry.Get<CreatureAnimation>(creature);
		const auto posed = TwoBoneBody();
		animation.skeleton = posed.skeleton;
		animation.mirror = posed.mirror;
		animation.boneMatrices = posed.boneMatrices;
		// the last frame drew the walk at its start
		animation.slots = {{.animation = 1, .timeMs = 0.0f, .weight = 1.0f, .mirrored = false}};
		animation.body.timeMs = 40.0f;
	}
	// the turn's end plays it half way through
	const CreatureAnimationSystem::TurnInputs inputs {
	    {creature, {.body = {}, .slots = {{.animation = 1, .timeMs = 500.0f, .weight = 1.0f, .mirrored = false}}}}};

	_system.PoseTurn(inputs);

	const auto& lookup = std::as_const(registry);
	const auto* pose = lookup.TryGet<const CreatureTurnPose>(creature);
	ASSERT_NE(pose, nullptr);
	auto sampler = lookup.Get<const CreatureAnimation>(creature);
	const auto& rig = *Locator::resources::value().GetCreatureRigs().Handle(creature::GetRigId(CreatureType::GiantApe));
	const auto& drawn = lookup.Get<const CreatureMorph>(creature).drawn;
	const auto& atEnd = inputs.front().second;
	const auto sampled = CreatureAnimationSystem::SampleBody(sampler, rig, drawn, atEnd.body, atEnd.slots);
	const auto& transform = lookup.Get<const Transform>(creature);
	const auto world = affine::Model(transform.position, transform.rotation, ecs::creature_pose::DrawnScale(lookup, creature));
	EXPECT_EQ(pose->current, creature_turn_pose::InWorld(sampled, world));
	// not the pose the frames last drew, nor what the body played before the turn's end
	const auto before = CreatureAnimationSystem::SampleBody(sampler, rig, drawn, sampler.body, sampler.slots);
	EXPECT_NE(sampled, k_TwoBoneRest);
	EXPECT_NE(sampled, before);
	// nothing is moved on: the frames' pose and what they play stay as they were
	const auto& animation = lookup.Get<const CreatureAnimation>(creature);
	EXPECT_EQ(animation.boneMatrices, k_TwoBoneRest);
	EXPECT_EQ(animation.body.timeMs, 40.0f);
	ASSERT_EQ(animation.slots.size(), 1u);
	EXPECT_EQ(animation.slots.front().timeMs, 0.0f);

	// a creature given nothing is sampled as it plays now
	const auto first = pose->current;
	_system.PoseTurn({});
	EXPECT_EQ(lookup.Get<const CreatureTurnPose>(creature).current, creature_turn_pose::InWorld(before, world));
	EXPECT_EQ(lookup.Get<const CreatureTurnPose>(creature).previous, first);
}
